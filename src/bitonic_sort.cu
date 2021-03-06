#include "cuda-util.h"
#include "gpu_graham_scan.h"

/*
 * Comparator between two points.  Used to see if p1 is "less" than p2.  This
 * function assumes that p1 and p2 both have positive x_ and y_ values.
 *
 * p1 is "less" than p2 if either of the following conditions hold:
 *   1) p1 is to the right of p2
 *   2) p1 and p2 are colinear, but p1 has a smaller squared magnitude than p2.
 */
template <class Num_Type>
__device__ bool comparePoints(const gpu_graham_scan::Point<Num_Type>& p1,
                              const gpu_graham_scan::Point<Num_Type>& p2) {
  // cross product of 2 points
  Num_Type x_product = (p1.x_ * p2.y_) - (p2.x_ * p1.y_);

  // right turn
  if (x_product > 0) {
    return true;
  }

  // colinear
  if (x_product == 0) {
    // square magnitude of each point
    Num_Type sq_mag_p1 = (p1.x_ * p1.x_) + (p1.y_ * p1.y_);
    Num_Type sq_mag_p2 = (p2.x_ * p2.x_) + (p2.y_ * p2.y_);
    return sq_mag_p1 < sq_mag_p2;
  }

  // left turn
  return false;
}

/*
 * Takes an array and splits it up into bitonic sub arrays.
 *
 * Args:
 *   n_points: the total number of points
 *   d_points: our array to be split up
 *   threads_per_chunk: threads per chunk/sub array (this means that the size of
 *                      each sub array is threads_per_chunk * 2)
 *   chunk_len: the length of our sub array.  This could be computed in the
 *              kernel but it is computed before hand to save additional
 *              computation
 */
template <class Num_Type>
__global__ void BuildBitonicKernel(size_t n_points,
                                   gpu_graham_scan::Point<Num_Type>* d_points,
                                   size_t threads_per_chunk, size_t chunk_len) {
  // should have 1/2 as many total threads as points
  size_t true_idx = threadIdx.x + (blockIdx.x * blockDim.x);
  size_t chunk_offset = (true_idx / threads_per_chunk) * chunk_len;
  size_t thread_offset = true_idx % threads_per_chunk;
  size_t first = thread_offset + chunk_offset;
  size_t last = (chunk_offset + chunk_len - 1) - thread_offset;
  if (last < n_points && comparePoints(d_points[last], d_points[first])) {
    gpu_graham_scan::Point<Num_Type> tmp = d_points[last];
    d_points[last] = d_points[first];
    d_points[first] = tmp;
  } else if (last < n_points &&
             !comparePoints(d_points[last], d_points[first])) {
  }
}

/*
 * Sorts an array comprised of bitonic subarrays.  We assign a single thread to
 * each swap location, thus if there are n indicies in our array, we round n to
 * the next largest power of two if n is not already a power of two, otherwise n
 * stays the same, then we let the total number of threads be this new number
 * divided by two
 *
 * Args:
 *   n_points: the total number of points
 *   d_points: our array to be sorted
 *   threads_per_chunk: threads per chunk/sub array (this means that the size of
 *                      each sub array is threads_per_chunk * 2)
 *   chunk_len: the length of our sub array.  This could be computed in the
 *              kernel but it is computed before hand to save additional
 *              computation
 */
template <class Num_Type>
__global__ void BitonicSortKernel(size_t n_points,
                                  gpu_graham_scan::Point<Num_Type>* d_points,
                                  size_t threads_per_chunk, size_t chunk_len) {
  // should have 1/2 as many total threads as points
  size_t true_idx = threadIdx.x + (blockIdx.x * blockDim.x);
  size_t chunk_offset = (true_idx / threads_per_chunk) * chunk_len;
  size_t thread_offset = true_idx % threads_per_chunk;
  size_t first = thread_offset + chunk_offset;
  size_t last = first + threads_per_chunk;
  if (last < n_points && comparePoints(d_points[last], d_points[first])) {
    gpu_graham_scan::Point<Num_Type> tmp = d_points[last];
    d_points[last] = d_points[first];
    d_points[first] = tmp;
  } else if (last < n_points &&
             !comparePoints(d_points[last], d_points[first])) {
  }
}

/*
 * Runs Bionic sort on an array of Point<Num_Type>
 *
 * Args:
 *   points_arr: our array to be sorted
 *   n_points: the total number of points
 */
template <class Num_Type>
void gpu_graham_scan::BitonicSortPoints(
    gpu_graham_scan::Point<Num_Type>* points_arr, size_t n_points) {
  const uint threads_per_block = 1024;  // Max threads_per_block = 1024;

  // Allocate device data
  gpu_graham_scan::Point<Num_Type>* d_points;

  cudaErrorCheck(cudaMalloc(
      &d_points, n_points * sizeof(gpu_graham_scan::Point<Num_Type>)));

  // points to device
  cudaErrorCheck(cudaMemcpy(d_points, points_arr,
                            n_points * sizeof(gpu_graham_scan::Point<Num_Type>),
                            cudaMemcpyHostToDevice));

  // round up to the the power of 2 to get our upper bound
  size_t upper_bound = n_points;
  uint power = 0;
  while (upper_bound) {
    upper_bound >>= 1;
    power++;
  }
  size_t curr_bound = 1 << (power - 1);
  upper_bound = (curr_bound < n_points) ? (curr_bound << 1) : curr_bound;

  size_t total_threads = upper_bound >> 1;
  for (size_t i = 2, j = i; i <= upper_bound; i *= 2, j = i) {
    size_t threads_per_chunk = j >> 1;
    BuildBitonicKernel<<<(total_threads + threads_per_block - 1) /
                             threads_per_block,
                         threads_per_block>>>(n_points, d_points,
                                              threads_per_chunk, j);

    // wait for build to finish
    cudaErrorCheck(cudaDeviceSynchronize());
    j >>= 1;
    while (j > 1) {
      threads_per_chunk = j >> 1;
      BitonicSortKernel<<<(total_threads + threads_per_block - 1) /
                              threads_per_block,
                          threads_per_block>>>(n_points, d_points,
                                               threads_per_chunk, j);
      cudaErrorCheck(cudaDeviceSynchronize());
      j >>= 1;
    }
  }

  // Copy points back to host points to device
  cudaErrorCheck(cudaMemcpy(points_arr, d_points,
                            n_points * sizeof(gpu_graham_scan::Point<Num_Type>),
                            cudaMemcpyDeviceToHost));

  // Cleanup device data
  cudaErrorCheck(cudaFree(d_points));
}

/*
 * Template implementations
 */

template void gpu_graham_scan::BitonicSortPoints(
    gpu_graham_scan::Point<int>* points_arr, size_t n_points);

template void gpu_graham_scan::BitonicSortPoints(
    gpu_graham_scan::Point<long>* points_arr, size_t n_points);

template void gpu_graham_scan::BitonicSortPoints(
    gpu_graham_scan::Point<float>* points_arr, size_t n_points);

template void gpu_graham_scan::BitonicSortPoints(
    gpu_graham_scan::Point<double>* points_arr, size_t n_points);

template void __global__
BuildBitonicKernel(size_t n_points, gpu_graham_scan::Point<int>* d_points,
                   size_t threads_per_chunk, size_t chunk_len);

template void __global__
BuildBitonicKernel(size_t n_points, gpu_graham_scan::Point<long>* d_points,
                   size_t threads_per_chunk, size_t chunk_len);

template void __global__
BuildBitonicKernel(size_t n_points, gpu_graham_scan::Point<float>* d_points,
                   size_t threads_per_chunk, size_t chunk_len);

template void __global__
BuildBitonicKernel(size_t n_points, gpu_graham_scan::Point<double>* d_points,
                   size_t threads_per_chunk, size_t chunk_len);

template __global__ void BitonicSortKernel(
    size_t n_points, gpu_graham_scan::Point<int>* d_points,
    size_t threads_per_chunk, size_t chunk_len);

template __global__ void BitonicSortKernel(
    size_t n_points, gpu_graham_scan::Point<long>* d_points,
    size_t threads_per_chunk, size_t chunk_len);

template __global__ void BitonicSortKernel(
    size_t n_points, gpu_graham_scan::Point<float>* d_points,
    size_t threads_per_chunk, size_t chunk_len);

template __global__ void BitonicSortKernel(
    size_t n_points, gpu_graham_scan::Point<double>* d_points,
    size_t threads_per_chunk, size_t chunk_len);

template __device__ bool comparePoints(const gpu_graham_scan::Point<int>& p1,
                                       const gpu_graham_scan::Point<int>& p2);

template __device__ bool comparePoints(const gpu_graham_scan::Point<long>& p1,
                                       const gpu_graham_scan::Point<long>& p2);

template __device__ bool comparePoints(const gpu_graham_scan::Point<float>& p1,
                                       const gpu_graham_scan::Point<float>& p2);

template __device__ bool comparePoints(
    const gpu_graham_scan::Point<double>& p1,
    const gpu_graham_scan::Point<double>& p2);

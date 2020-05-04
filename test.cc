#include <iostream>

#include "CycleTimer.h"
#include "gpu_graham_scan.h"

using namespace std;
using namespace gpu_graham_scan;

int kRuns = 3;

/*
 * find convex hull for points in input
 *
 */
std::vector<int> SolveSerial(GrahamScanSerial<int>* input) {
  cout << "Solving serial\n";
  input->CenterP0();
  sort(input->points_.begin(), input->points_.end());
  std::vector<int> hull_out = input->Run();
  return hull_out;
}

bool ValidateSolution(const std::vector<int> soln1,
                      const std::vector<int> soln2) {
  cout << "Validating Solution \n";
  cout << "soln1 size: " << soln1.size();
  if (soln1.size() != soln2.size()) {
    fprintf(stderr, "Hulls have different numbers of elements");
    return false;
  }

  for (int i = 0; i < soln1.size(); i++) {
    if (i < soln1.size() && i < soln2.size()) {
      if (soln1[i] != soln2[i]) {
        return false;
      }
    }
  }
  return true;
}

template <class Fn>
double Benchmark(int num_runs, Fn&& fn, const char* filename,
                 std::vector<int>& hull) {
  double min_time = std::numeric_limits<double>::max();
  for (int i = 0; i < num_runs; i++) {
    GrahamScanSerial<int> input(filename);
    double start_time = CycleTimer::currentSeconds();
    std::vector<int> temp = fn(&input);
    hull = temp;
    cout << "benchmark hull size: " << hull.size() << "\n";
    double end_time = CycleTimer::currentSeconds();
    min_time = std::min(min_time, end_time - start_time);
  }
  return min_time;
}

int main() {
  // Point<int> p1;
  // p1.x = 1;
  // p1.y = 2;

  GrahamScanSerial<int> test("test-data/test1.in");
  cout << "constructed \n";

  std::vector<int> output;
  // output = std::move(output);
  // std::vector<int>&& output_vec = output;
  // output = SolveSerial(&test);
  // cout << "solve serial:" << output.size() << "\n";

  double min_time = Benchmark(kRuns, SolveSerial, "test-data/test1.in", output);
  cout << "min time was: " << min_time << " seconds\n";
  cout << "validate solution\n";
  cout << output.size() << "\n";
  // std::vector<int>&output_ref = *output; cout << ValidateSolution(*output,
  // *output);

  // Benchmark(kRuns, Run)
  // cout << "test min point: " << test.GetMinYPoint().x << ' ' <<
  // test.GetMinYPoint().y << '\n'; cout << "true min point: -1 -1 \n"; cout <<
  // "num points: " << test.points_.size() << "\n"; Point<int> p2; p2.x = 1;
  // p2.y = 1;

  // cout << "polar angle: p2 -> p1 " << test.PolarAngle(p2, p1) <<  "\n";

  // Point<int> p3;
  // p3.x = 2;
  // p3.y = 1;

  // cout << "non left turn: " << test.NonLeftTurn(p1, p3, p2) << "\n";
  // cout << "non left turn: " << test.NonLeftTurn(p1, p2, p3) << "\n";

  // test.CenterP0(test.GetMinYPoint());

  // cout << "begin for loop\n";
  // for (int i = 0; i < test.points_.size(); i++) {
  //   cout << i << "\n";
  //   Point<int> current_point = test.points_[i];
  //   std::cout << "x, y: " << current_point.x << " " << current_point.y <<
  //   "\n";
  // }
}

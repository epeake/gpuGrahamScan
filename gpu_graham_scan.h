#include <algorithm>

#include "CycleTimer.h"
#pragma once

#ifndef _GPU_Graham_Scan_
#define _GPU_Graham_Scan_

/*
 * prints helpful error diagnostics
 */
#define GPU_GS_PRINT_ERR(message)                                          \
  fprintf(stderr, "Error: function %s, file %s, line %d.\n%s\n", __func__, \
          __FILE__, __LINE__, message);
#define GPU_GS_PRINT_ERR_LOC()                                         \
  fprintf(stderr, "Error: function %s, file %s, line %d.\n", __func__, \
          __FILE__, __LINE__);

#include <errno.h>
#include <stdio.h>

#include <fstream>
#include <stack>
#include <string>
#include <vector>

namespace gpu_graham_scan {

/*
 * Cartesian Coordinate Point
 */
template <class Num_Type>
struct Point {
  Num_Type x;
  Num_Type y;
  int id;
};

enum TurnDir { RIGHT, NONE, LEFT };

/*
 * subtract two Points from each other
 */
template <class Num_Type>
Point<Num_Type> operator-(const Point<Num_Type>& p1,
                          const Point<Num_Type>& p2) {
  Point<Num_Type> tmp;
  tmp.x = p1.x - p2.x;
  tmp.y = p1.y - p2.y;
  return tmp;
}

/*
 * add two Points from each other
 */
template <class Num_Type>
Point<Num_Type> operator+(const Point<Num_Type>& p1,
                          const Point<Num_Type>& p2) {
  Point<Num_Type> tmp;
  tmp.x = p1.x + p2.x;
  tmp.y = p1.y + p2.y;
  return tmp;
}

template <class Num_Type>
bool operator<(const Point<Num_Type>& a, const Point<Num_Type>& b) {
  return XProduct(a, b) <= 0;
}

/*
 * Calculate the cross product between two Points
 */
template <class Num_Type>
float XProduct(const Point<Num_Type>& p1, const Point<Num_Type>& p2) {
  return ((p2.x) * (p1.y)) - ((p1.x) * (p2.y));
}

/*
 * Used to read in a file of Point to be stored
 * as a vector.
 */
template <class Num_Type>
class GrahamScanSerial {
 public:
  /*
   * Constructor reads through the file, populating points_ and p0_
   */
  GrahamScanSerial(const char* filename) : filename_(filename) { ReadFile(); };

  ~GrahamScanSerial() {}

  /*
   * filename of points to be read in
   */
  std::string filename_;

  /*
   * all of our points from the file
   */
  std::vector<Point<Num_Type> > points_;

  /*
   * identifies direction of turn using origin, p1, p2
   */
  int Turn(Point<Num_Type> p1, Point<Num_Type> p2) const {
    float x_product = XProduct(p1, p2);
    if (x_product > 0) {  // right turn
      return RIGHT;
    }
    if (x_product == 0) {  // equivalent angles
      return NONE;
    }
    return LEFT;  // left turn
  }

  /*
   * does the ordering self, p1, p2 create a non-left turn
   *
   * args: three Points that form a turn
   * returns: if p0->p2 is a non-left turn relative to p0->p1
   */
  bool NonLeftTurn(Point<Num_Type> p0, Point<Num_Type> p1,
                   Point<Num_Type> p2) const {
    return XProduct(p1 - p0, p2 - p0) > 0;
  };

  void CenterP0() {
    for (int i = 0; i < points_.size(); i++) {
      points_[i] = points_[i] - p0_;
    }
  }

  std::vector<int> Run() {
    std::stack<Point<Num_Type> > s;
    s.push(points_[0]);
    s.push(points_[1]);
    s.push(points_[2]);
    // std::cout << "initial points pushed\n";
    Point<Num_Type> top1, top2, current_point;
    for (int i = 3; i < points_.size(); i++) {
      top1 = s.top();
      s.pop();
      top2 = s.top();
      current_point = points_[i];
      while (Turn(top1 - top2, current_point - top2) != LEFT) {
        top1 = s.top();
        s.pop();
        top2 = s.top();
      }
      s.push(top1);
      s.push(current_point);
    }
    // std::cout << "algorithm run\n";

    std::vector<int> hull;
    while (!s.empty()) {
      current_point = s.top();
      hull.push_back(current_point.id);
      s.pop();
    }
    // std::cout << "while completed\n";
    return hull;
  }

 private:
  Point<Num_Type> p0_;

  GrahamScanSerial(void);

  void ReadFile() {
    // only function that throws is stoi/stod so need try/catch
    try {
      std::string curr_line;
      std::ifstream infile;
      int idx = 0;

      infile.open(filename_);
      if (errno != 0) {
        GPU_GS_PRINT_ERR_LOC();
        perror("infile.open");
        exit(EXIT_FAILURE);
      }

      // get number of points from first line of file
      getline(infile, curr_line);
      if (infile.fail()) {
        GPU_GS_PRINT_ERR_LOC();
        perror("getline");
        exit(EXIT_FAILURE);
      }

      int total_points = stoi(curr_line);
      if (errno != 0) {
        std::cout << "hi";
      }

      if (total_points < 4) {
        GPU_GS_PRINT_ERR("Less than four points in input file");
        exit(EXIT_FAILURE);
      }

      points_.resize(total_points);

      // process each line individually of the file
      Point<Num_Type> curr_min;
      while (!infile.eof()) {
        std::cout << "idx = " << idx << " " << points_.size() << "\n";
        getline(infile, curr_line);
        if (infile.fail()) {
          GPU_GS_PRINT_ERR_LOC();
          perror("getline");
          exit(EXIT_FAILURE);
        }
        int comma = curr_line.find(',');
        std::string first_num = curr_line.substr(0, comma);
        std::string second_num =
            curr_line.substr(comma + 1, curr_line.length());

        Point<Num_Type> current_point;
        current_point.x = static_cast<Num_Type>(stod(first_num));
        current_point.y = static_cast<Num_Type>(stod(second_num));
        current_point.id = idx;

        // update the current minumim point's index

        if (idx == 0 ||
            (current_point.y == curr_min.y && current_point.x < curr_min.x) ||
            current_point.y < curr_min.y) {
          curr_min = current_point;
        }

        points_[idx] = current_point;
        idx++;
      }

      p0_ = curr_min;

      if (total_points != idx) {
        GPU_GS_PRINT_ERR("Incorrect number of points specified by file");
        exit(EXIT_FAILURE);
      }

      infile.close();
      if (infile.fail()) {
        GPU_GS_PRINT_ERR_LOC();
        perror("infile.close");
        exit(EXIT_FAILURE);
      }
    } catch (const std::out_of_range& oor) {
      GPU_GS_PRINT_ERR(oor.what());
      exit(EXIT_FAILURE);
    } catch (const std::invalid_argument& ia) {
      GPU_GS_PRINT_ERR(ia.what());
      exit(EXIT_FAILURE);
    }
  }
};

}  // namespace gpu_graham_scan

#endif  // _GPU_Graham_Scan_

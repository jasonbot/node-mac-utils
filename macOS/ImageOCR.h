#pragma once
#include <string>
#include <vector>

class Rectangle {
public:
  size_t x, y, width, height;
};

class OCRObservation {
public:
  Rectangle bbox;
  std::string text;
};

class OCRData {
public:
  bool success;
  std::vector<OCRObservation> observations;
  OCRData(const std::vector<OCRObservation> &observations)
      : success(true), observations(observations) {}
};

OCRData OCRImageData(const std::vector<uint8_t> &);

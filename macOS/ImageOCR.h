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
  OCRObservation(const Rectangle &bbox, const std::string &text)
      : bbox(bbox), text(text) {};
};

class OCRData {
public:
  bool success;
  std::vector<OCRObservation> observations;
  OCRData(std::vector<OCRObservation> &observations)
      : success(true), observations(observations) {}
  OCRData(const bool &success) : success(success) {}
};

OCRData OCRImageData(const std::vector<uint8_t> &);

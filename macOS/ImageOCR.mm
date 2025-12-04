#include <CoreGraphics/CGImage.h>
#include <Foundation/Foundation.h>
#include <Vision/Vision.h>

#include "./ImageOCR.h"

OCRData OCRImageData(const std::vector<uint8_t> &data) {
  auto length(data.size());
  auto rawData(data.data());
  CGDataProviderRef prov(
      CGDataProviderCreateWithData(nullptr, rawData, length, nullptr));
  if (prov == nullptr) {
    return OCRData(false);
  }
  std::vector<OCRObservation> observations;

  @autoreleasepool {
    auto image(CGImageCreateWithPNGDataProvider(prov, NULL, true,
                                                kCGRenderingIntentDefault));
    CGDataProviderRelease(prov);
    if (image == nullptr) {
      return OCRData(false);
    }

    auto options([NSDictionary alloc]);
    auto handler([[VNImageRequestHandler alloc] initWithCGImage:image
                                                        options:options]);

    if (handler != nullptr) {
      auto request([[VNRecognizeTextRequest alloc] init]);
      NSError *err(nullptr);
      [handler performRequests:@[ request ] error:&err];

      if (err == nullptr) {
        auto results([request results]);

        if (results != nullptr) {
          auto width(CGImageGetWidth(image)), height(CGImageGetHeight(image));
          for (NSUInteger i(0); i < [results count]; ++i) {
            auto result([results objectAtIndex:i]);
            auto bbox(VNImageRectForNormalizedRect([result boundingBox], width,
                                                   height));
            auto firstObject([[result topCandidates:1] firstObject]);
            if (firstObject != nullptr && [firstObject string] != nullptr) {
              const auto rect(
                  Rectangle{size_t(bbox.origin.x), size_t(bbox.origin.y),
                            size_t(bbox.size.width), size_t(bbox.size.height)});
              const std::string text([[firstObject string] UTF8String]);
              observations.emplace_back(rect, text);
            }
          }
        }
      }
    }
    CGImageRelease(image);
    return OCRData(observations);
  }
}

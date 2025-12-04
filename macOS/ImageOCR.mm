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
    return OCRData();
  }
  auto image(CGImageCreateWithPNGDataProvider(prov, NULL, true,
                                              kCGRenderingIntentDefault));
  CGDataProviderRelease(prov);
  if (image == nullptr) {
    return OCRData();
  }

  auto options([NSDictionary alloc]);
  auto handler([[VNImageRequestHandler alloc] initWithCGImage:image
                                                      options:options]);

  if (handler != nullptr) {
    auto request([[VNRecognizeTextRequest alloc]
        initWithCompletionHandler:[](VNRequest *_Nonnull request,
                                     NSError *_Nullable error) {}]);
    NSError *err(nullptr);
    [handler performRequests:@[ request ] error:&err];
    if (err == nullptr) {
      auto results([request results]);
      if (results != nullptr) {
        for (NSUInteger i(0); i < [results count]; ++i) {
          auto result([results objectAtIndex:i]);
          [result boundingBox];
        }
      }
    }
  }

  CGImageRelease(image);
  return OCRData();
}

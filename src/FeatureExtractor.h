#ifndef FEATURE_EXTRACTOR_H
#define FEATURE_EXTRACTOR_H

#include <Arduino.h> 
#include <numeric>   
#include <cmath>     

const int FE_WINDOW_SIZE = 50; 

class FeatureExtractor {
public:
    FeatureExtractor();
    void addSample(float voltage, float current);
    void extractFeatures(float& meanV, float& stdV, float& stdC, float& meanC); // Perubahan urutan parameter
    bool isReady();
    int getCurrentSampleCount() const;

private:
    float _voltageWindow[FE_WINDOW_SIZE];
    float _currentWindow[FE_WINDOW_SIZE];
    int _windowIndex;
    bool _isWindowFull;

    float _calculateMean(const float data[], int size);
    float _calculateStdDev(const float data[], int size, float mean);
};

#endif // FEATURE_EXTRACTOR_H
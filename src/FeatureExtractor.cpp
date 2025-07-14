#include "FeatureExtractor.h"

FeatureExtractor::FeatureExtractor() {
    _windowIndex = 0;
    _isWindowFull = false;
    for (int i = 0; i < FE_WINDOW_SIZE; i++) {
        _voltageWindow[i] = 0.0;
        _currentWindow[i] = 0.0;
    }
}

void FeatureExtractor::addSample(float voltage, float current) {
    _voltageWindow[_windowIndex] = voltage;
    _currentWindow[_windowIndex] = current;
    
    _windowIndex++;
    if (_windowIndex >= FE_WINDOW_SIZE) {
        _windowIndex = 0; 
        _isWindowFull = true; 
    }
}

// Perhatikan urutan parameter di sini agar sesuai dengan deklarasi di .h dan kebutuhan model:
// meanV, stdV, meanC, stdC
void FeatureExtractor::extractFeatures(float& meanV, float& stdV, float& meanC, float& stdC) {
    if (!_isWindowFull) {
        // Jika jendela belum penuh, nilai yang dihitung mungkin tidak stabil/akurat.
        // Sebaiknya selalu panggil isReady() sebelum memanggil ini.
    }

    meanV = _calculateMean(_voltageWindow, FE_WINDOW_SIZE);
    stdV = _calculateStdDev(_voltageWindow, FE_WINDOW_SIZE, meanV);
    
    meanC = _calculateMean(_currentWindow, FE_WINDOW_SIZE);
    stdC = _calculateStdDev(_currentWindow, FE_WINDOW_SIZE, meanC);
}

bool FeatureExtractor::isReady() {
    return _isWindowFull;
}

int FeatureExtractor::getCurrentSampleCount() const {
    if (_isWindowFull) {
        return FE_WINDOW_SIZE;
    }
    return _windowIndex;
}

float FeatureExtractor::_calculateMean(const float data[], int size) {
    double sum = 0.0;
    for (int i = 0; i < size; i++) {
        sum += data[i];
    }
    return static_cast<float>(sum / size);
}

float FeatureExtractor::_calculateStdDev(const float data[], int size, float mean) {
    if (size < 2) return 0.0; 
    double sum_sq_diff = 0.0;
    for (int i = 0; i < size; i++) {
        sum_sq_diff += pow(data[i] - mean, 2);
    }
    return static_cast<float>(sqrt(sum_sq_diff / (size - 1)));
}
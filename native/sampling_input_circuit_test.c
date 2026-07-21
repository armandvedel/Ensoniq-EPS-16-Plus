#include "sampling_input_circuit.h"

#include <math.h>
#include <stdio.h>

static double measure(double sample_rate, double frequency, int microphone,
                      double amplitude) {
    SamplingInputCircuit circuit;
    sampling_input_circuit_init(&circuit, sample_rate);
    const unsigned int count = (unsigned int)(sample_rate * 0.5);
    double sum = 0.0;
    unsigned int measured = 0;
    for (unsigned int sample = 0; sample < count; ++sample) {
        const double input = amplitude *
            sin(2.0 * 3.14159265358979323846 * frequency * sample / sample_rate);
        const double output = sampling_input_circuit_process(
            &circuit, (float)input, microphone);
        if (sample >= count / 2) {
            sum += output * output;
            ++measured;
        }
    }
    return sqrt(sum / measured) * 1.4142135623730950488 / amplitude;
}

int main(void) {
    const double line_1k = measure(48000.0, 1000.0, 0, 0.01);
    const double mic_1k = measure(48000.0, 1000.0, 1, 0.01);
    const double line_10k = measure(48000.0, 10000.0, 0, 0.01);
    const double line_1k_96 = measure(96000.0, 1000.0, 0, 0.01);
    printf("line_1k=%f mic_1k=%f ratio=%f line_10k=%f line_1k_96=%f\n",
           line_1k, mic_1k, mic_1k / line_1k, line_10k, line_1k_96);
    if (!(line_1k > 2.22 && line_1k < 2.32)) return 1;
    if (!(mic_1k / line_1k > 24.5 && mic_1k / line_1k < 25.4)) return 1;
    if (!(line_10k > 1.30 && line_10k < 1.50)) return 1;
    if (!(fabs(line_1k_96 - line_1k) < 0.025)) return 1;

    SamplingInputCircuit clipping;
    sampling_input_circuit_init(&clipping, 48000.0);
    float peak = 0.0f;
    for (unsigned int sample = 0; sample < 48000; ++sample) {
        const float input = 0.25f * sinf((float)sample * 0.17f);
        const float output = sampling_input_circuit_process(&clipping, input, 1);
        if (fabsf(output) > peak) peak = fabsf(output);
    }
    if (!(peak <= 1.0f && peak > 0.99f)) return 1;
    return 0;
}

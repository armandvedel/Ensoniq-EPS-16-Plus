#ifndef EPS16_SAMPLING_INPUT_CIRCUIT_H
#define EPS16_SAMPLING_INPUT_CIRCUIT_H

typedef struct {
    double b;
    double a1;
    double x1;
    double y1;
} SamplingInputOnePole;

typedef struct {
    double b[4];
    double a[3];
    double state[3];
} SamplingInputThirdOrder;

typedef struct {
    double sample_rate;
    unsigned int oversample;
    double input_history[3];
    unsigned int input_history_count;
    SamplingInputOnePole input_coupling;
    SamplingInputOnePole feedback_capacitor;
    SamplingInputOnePole filter_coupling;
    SamplingInputThirdOrder fixed_filter[2];
} SamplingInputCircuit;

void sampling_input_circuit_init(SamplingInputCircuit *circuit,
                                 double sample_rate);
void sampling_input_circuit_set_rate(SamplingInputCircuit *circuit,
                                     double sample_rate);
void sampling_input_circuit_reset(SamplingInputCircuit *circuit);
float sampling_input_circuit_process(SamplingInputCircuit *circuit,
                                     float input, int microphone);

#endif

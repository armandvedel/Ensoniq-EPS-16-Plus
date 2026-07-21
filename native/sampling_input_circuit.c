/*
 * EPS-16 Plus sampling-input analog circuit.
 *
 * This is a component-derived linear model of schematic sheet 3: the TL072
 * input amplifier and CD4053 LINE/MIC feedback network, followed by the two
 * identical buffered R/C filter sections. The ADC clip is retained, while
 * unmeasured noise and guessed op-amp coloration are deliberately omitted.
 */
#include "sampling_input_circuit.h"

#include <math.h>
#include <string.h>

static void one_pole_prepare(SamplingInputOnePole *filter, double resistance,
                             double capacitance, double sample_rate) {
    const double pole = 1.0 / (resistance * capacitance);
    const double denominator = 2.0 * sample_rate + pole;
    filter->b = pole / denominator;
    filter->a1 = (pole - 2.0 * sample_rate) / denominator;
}

static double one_pole_lowpass(SamplingInputOnePole *filter, double input) {
    const double output = filter->b * (input + filter->x1) -
                          filter->a1 * filter->y1;
    filter->x1 = input;
    filter->y1 = output;
    return output;
}

static double one_pole_highpass(SamplingInputOnePole *filter, double input) {
    return input - one_pole_lowpass(filter, input);
}

static void third_order_prepare(SamplingInputThirdOrder *filter,
                                double sample_rate) {
    /* One complete buffered filter section:

         R0=R1=R2=1.78k, C0=4700p, C2=470p, Cf=8200p+8200p.

       Solving its three physical nodes gives the normalized denominator
       1 + a1*s + a2*s^2 + a3*s^3. This includes the loading of the first RC
       node by the Sallen-Key network; treating them as independent filters
       produces the wrong resonance. */
    const double resistance = 1780.0;
    const double c0 = 4700.0e-12;
    const double c2 = 470.0e-12;
    const double cf = 2.0 * 8200.0e-12;
    const double analog1 = resistance * (c0 + 3.0 * c2);
    const double analog2 = 2.0 * resistance * resistance * c2 * (cf + c0);
    const double analog3 = resistance * resistance * resistance * c0 * cf * c2;
    const double k = 2.0 * sample_rate;
    const double t1 = analog1 * k;
    const double t2 = analog2 * k * k;
    const double t3 = analog3 * k * k * k;
    const double d0 = 1.0 + t1 + t2 + t3;
    const double d1 = 3.0 + t1 - t2 - 3.0 * t3;
    const double d2 = 3.0 - t1 - t2 + 3.0 * t3;
    const double d3 = 1.0 - t1 + t2 - t3;
    filter->b[0] = 1.0 / d0;
    filter->b[1] = 3.0 / d0;
    filter->b[2] = 3.0 / d0;
    filter->b[3] = 1.0 / d0;
    filter->a[0] = d1 / d0;
    filter->a[1] = d2 / d0;
    filter->a[2] = d3 / d0;
}

static double third_order_process(SamplingInputThirdOrder *filter,
                                  double input) {
    const double output = filter->b[0] * input + filter->state[0];
    filter->state[0] = filter->b[1] * input - filter->a[0] * output +
                       filter->state[1];
    filter->state[1] = filter->b[2] * input - filter->a[1] * output +
                       filter->state[2];
    filter->state[2] = filter->b[3] * input - filter->a[2] * output;
    return output;
}

void sampling_input_circuit_reset(SamplingInputCircuit *circuit) {
    if (!circuit) return;
    memset(circuit->input_history, 0, sizeof(circuit->input_history));
    circuit->input_history_count = 0;
    circuit->input_coupling.x1 = circuit->input_coupling.y1 = 0.0;
    circuit->feedback_capacitor.x1 = circuit->feedback_capacitor.y1 = 0.0;
    circuit->filter_coupling.x1 = circuit->filter_coupling.y1 = 0.0;
    memset(circuit->fixed_filter[0].state, 0,
           sizeof(circuit->fixed_filter[0].state));
    memset(circuit->fixed_filter[1].state, 0,
           sizeof(circuit->fixed_filter[1].state));
}

void sampling_input_circuit_set_rate(SamplingInputCircuit *circuit,
                                     double sample_rate) {
    if (!circuit || !isfinite(sample_rate) || sample_rate < 8000.0 ||
        sample_rate > 768000.0)
        return;
    circuit->sample_rate = sample_rate;
    circuit->oversample = sample_rate < 64000.0 ? 4U
                            : sample_rate < 128000.0 ? 2U : 1U;
    const double internal_rate = sample_rate * circuit->oversample;
    /* C127=1uF sees the 470-ohm protection resistor plus R135=100k. */
    one_pole_prepare(&circuit->input_coupling, 100470.0, 1.0e-6,
                     internal_rate);
    /* C128=100pF is in parallel with R137=220k. */
    one_pole_prepare(&circuit->feedback_capacitor, 220000.0, 100.0e-12,
                     internal_rate);
    /* C148/C149 are equal back-to-back 10uF capacitors: 5uF effective. */
    one_pole_prepare(&circuit->filter_coupling, 100000.0, 5.0e-6,
                     internal_rate);
    third_order_prepare(&circuit->fixed_filter[0], internal_rate);
    third_order_prepare(&circuit->fixed_filter[1], internal_rate);
    sampling_input_circuit_reset(circuit);
}

void sampling_input_circuit_init(SamplingInputCircuit *circuit,
                                 double sample_rate) {
    if (!circuit) return;
    memset(circuit, 0, sizeof(*circuit));
    sampling_input_circuit_set_rate(circuit, sample_rate);
}

static double circuit_step(SamplingInputCircuit *circuit, double input,
                           int microphone) {
    input = one_pole_highpass(&circuit->input_coupling, input);

    /* U51/B feedback network. In LINE, R121 connects the junction to the
       summing node; in MIC it grounds the junction and creates the bridged-T
       gain. Retain the physical gain before the ADC: the original ES5510
       sampling filter has its own passband scaling, so normalizing LINE here
       incorrectly attenuates the sample written by the OS. */
    const double r136 = 220000.0;
    const double r137 = 220000.0;
    const double r121 = 9100.0;
    const double rin = 100000.0;
    const double parallel = 1.0 / (1.0 / r136 + 1.0 / r121);
    const double low = one_pole_lowpass(&circuit->feedback_capacitor, input);
    double output;
    if (microphone) {
        const double high_gain = r136 / rin;
        const double dc_gain =
            (r136 + r137 + r136 * r137 / r121) / rin;
        output = high_gain * input + (dc_gain - high_gain) * low;
    } else {
        const double high_gain = parallel / rin;
        const double dc_gain = (parallel + r137) / rin;
        output = high_gain * input + (dc_gain - high_gain) * low;
    }

    output = one_pole_highpass(&circuit->filter_coupling, output);
    output = third_order_process(&circuit->fixed_filter[0], output);
    return third_order_process(&circuit->fixed_filter[1], output);
}

float sampling_input_circuit_process(SamplingInputCircuit *circuit,
                                     float input, int microphone) {
    if (!circuit || !circuit->oversample) return input;
    double current = input;
    if (current > 1.0) current = 1.0;
    if (current < -1.0) current = -1.0;
    double output = 0.0;
    if (circuit->input_history_count < 3) {
        circuit->input_history[circuit->input_history_count++] = current;
        for (unsigned int step = 0; step < circuit->oversample; ++step)
            output = circuit_step(circuit, current, microphone);
        if (output > 1.0) output = 1.0;
        if (output < -1.0) output = -1.0;
        return (float)output;
    }
    /* Reconstruct the interval ending one host sample ago with a four-point
       Catmull-Rom segment. This avoids the audible high-frequency droop of
       linear interpolation while the one-sample analog-input delay is far
       below the EPS sampling/control latency. */
    const double x0 = circuit->input_history[0];
    const double x1 = circuit->input_history[1];
    const double x2 = circuit->input_history[2];
    const double x3 = current;
    for (unsigned int step = 1; step <= circuit->oversample; ++step) {
        const double t = (double)step / (double)circuit->oversample;
        const double t2 = t * t;
        const double t3 = t2 * t;
        const double reconstructed = 0.5 *
            (2.0 * x1 + (-x0 + x2) * t +
             (2.0 * x0 - 5.0 * x1 + 4.0 * x2 - x3) * t2 +
             (-x0 + 3.0 * x1 - 3.0 * x2 + x3) * t3);
        output = circuit_step(circuit, reconstructed, microphone);
    }
    circuit->input_history[0] = x1;
    circuit->input_history[1] = x2;
    circuit->input_history[2] = x3;
    /* The converter is the first known hard full-scale boundary. */
    if (output > 1.0) output = 1.0;
    if (output < -1.0) output = -1.0;
    return (float)output;
}

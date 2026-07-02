#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <complex>
#include <vector>

/**
    ToneStack — a physically-modelled passive guitar-amp tone stack (the
    interactive Bass / Mid / Treble network). Unlike independent EQ shelves,
    the three controls load each other exactly as they do in the real circuit,
    which is what gives an amp its recognisable voice.

    The analog transfer function H(s) = B(s)/A(s) is built from the actual
    component values, then discretised with the bilinear transform into a 3rd
    order IIR run per channel (Direct Form II transposed).

    Component values + coefficient formulas from the Faust tonestacks library.
*/
class ToneStack
{
public:
    enum class Type { Marshall, Fender };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate  = spec.sampleRate;
        numChannels = (int) spec.numChannels;
        z0.assign ((size_t) numChannels, 0.0);
        z1.assign ((size_t) numChannels, 0.0);
        z2.assign ((size_t) numChannels, 0.0);
        dirty = true;
        update();
    }

    void reset()
    {
        std::fill (z0.begin(), z0.end(), 0.0);
        std::fill (z1.begin(), z1.end(), 0.0);
        std::fill (z2.begin(), z2.end(), 0.0);
    }

    void setType (Type t)                        { if (t != type) { type = t; dirty = true; } }
    void setParams (float bass, float mid, float treble)
    {
        if (bass != lB || mid != mB || treble != tB) { lB = bass; mB = mid; tB = treble; dirty = true; }
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (dirty)
            update();

        const int nCh = juce::jmin ((int) block.getNumChannels(), numChannels);
        const int nS  = (int) block.getNumSamples();

        for (int ch = 0; ch < nCh; ++ch)
        {
            double s0 = z0[(size_t) ch], s1 = z1[(size_t) ch], s2 = z2[(size_t) ch];
            for (int i = 0; i < nS; ++i)
            {
                const double x = (double) block.getSample (ch, i) * makeup;
                const double y = b0 * x + s0;
                s0 = b1 * x - a1 * y + s1;
                s1 = b2 * x - a2 * y + s2;
                s2 = b3 * x - a3 * y;
                block.setSample (ch, i, (float) y);
            }
            z0[(size_t) ch] = s0; z1[(size_t) ch] = s1; z2[(size_t) ch] = s2;
        }
    }

private:
    void components (double& R1, double& R2, double& R3, double& R4,
                     double& C1, double& C2, double& C3) const
    {
        if (type == Type::Marshall)   // JCM800
        {
            R1 = 220e3; R2 = 1e6; R3 = 22e3; R4 = 33e3;
            C1 = 470e-12; C2 = 22e-9; C3 = 22e-9;
        }
        else                          // Fender Bassman
        {
            R1 = 250e3; R2 = 1e6; R3 = 25e3; R4 = 56e3;
            C1 = 250e-12; C2 = 20e-9; C3 = 20e-9;
        }
    }

    void analogCoeffs (double t, double m, double l,
                       std::array<double, 4>& B, std::array<double, 4>& A) const
    {
        double R1, R2, R3, R4, C1, C2, C3;
        components (R1, R2, R3, R4, C1, C2, C3);

        B[0] = 0.0;
        B[1] = t*C1*R1 + m*C3*R3 + l*(C1*R2 + C2*R2) + (C1*R3 + C2*R3);
        B[2] = t*(C1*C2*R1*R4 + C1*C3*R1*R4) - m*m*(C1*C3*R3*R3 + C2*C3*R3*R3)
             + m*(C1*C3*R1*R3 + C1*C3*R3*R3 + C2*C3*R3*R3)
             + l*(C1*C2*R1*R2 + C1*C2*R2*R4 + C1*C3*R2*R4)
             + l*m*(C1*C3*R2*R3 + C2*C3*R2*R3)
             + (C1*C2*R1*R3 + C1*C2*R3*R4 + C1*C3*R3*R4);
        B[3] = l*m*(C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4)
             - m*m*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4)
             + m*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4)
             + t*C1*C2*C3*R1*R3*R4 - t*m*C1*C2*C3*R1*R3*R4
             + t*l*C1*C2*C3*R1*R2*R4;

        A[0] = 1.0;
        A[1] = (C1*R1 + C1*R3 + C2*R3 + C2*R4 + C3*R4) + m*C3*R3 + l*(C1*R2 + C2*R2);
        A[2] = m*(C1*C3*R1*R3 - C2*C3*R3*R4 + C1*C3*R3*R3 + C2*C3*R3*R3)
             + l*m*(C1*C3*R2*R3 + C2*C3*R2*R3) - m*m*(C1*C3*R3*R3 + C2*C3*R3*R3)
             + l*(C1*C2*R2*R4 + C1*C2*R1*R2 + C1*C3*R2*R4 + C2*C3*R2*R4)
             + (C1*C2*R1*R4 + C1*C3*R1*R4 + C1*C2*R3*R4 + C1*C2*R1*R3 + C1*C3*R3*R4 + C2*C3*R3*R4);
        A[3] = l*m*(C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4)
             - m*m*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4)
             + m*(C1*C2*C3*R3*R3*R4 + C1*C2*C3*R1*R3*R3 - C1*C2*C3*R1*R3*R4)
             + l*C1*C2*C3*R1*R2*R4 + C1*C2*C3*R1*R3*R4;
    }

    void update()
    {
        std::array<double, 4> B, A;
        analogCoeffs ((double) tB, (double) mB, (double) lB, B, A);
        bilinear (B, A);

        // Calibrate insertion loss: normalise the level at 1 kHz for this
        // amp's noon settings so switching amps doesn't jump the volume.
        std::array<double, 4> Bn, An;
        analogCoeffs (0.5, 0.5, 0.5, Bn, An);
        const double mag = analogMag (Bn, An, 2.0 * juce::MathConstants<double>::pi * 1000.0);
        makeup = mag > 1e-9 ? 1.0 / mag : 1.0;

        dirty = false;
    }

    void bilinear (const std::array<double, 4>& B, const std::array<double, 4>& A)
    {
        const double c = 2.0 * sampleRate;
        const double cp[4] = { 1.0, c, c * c, c * c * c };

        // Coefficients of (1 - z^-1)^k (1 + z^-1)^(3-k) for k = 0..3.
        static const double P[4][4] = {
            {  1,  3,  3,  1 },
            {  1,  1, -1, -1 },
            {  1, -1, -1,  1 },
            {  1, -3,  3, -1 } };

        double Bd[4] = { 0, 0, 0, 0 };
        double Ad[4] = { 0, 0, 0, 0 };
        for (int k = 0; k < 4; ++k)
            for (int j = 0; j < 4; ++j)
            {
                Bd[j] += B[(size_t) k] * cp[k] * P[k][j];
                Ad[j] += A[(size_t) k] * cp[k] * P[k][j];
            }

        const double a0 = Ad[0];
        b0 = Bd[0] / a0; b1 = Bd[1] / a0; b2 = Bd[2] / a0; b3 = Bd[3] / a0;
        a1 = Ad[1] / a0; a2 = Ad[2] / a0; a3 = Ad[3] / a0;
    }

    static double analogMag (const std::array<double, 4>& B, const std::array<double, 4>& A, double w)
    {
        const std::complex<double> jw (0.0, w);
        const std::complex<double> num = B[0] + B[1]*jw + B[2]*jw*jw + B[3]*jw*jw*jw;
        const std::complex<double> den = A[0] + A[1]*jw + A[2]*jw*jw + A[3]*jw*jw*jw;
        return std::abs (num / den);
    }

    double sampleRate  = 48000.0;
    int    numChannels = 2;
    Type   type = Type::Marshall;

    float lB = 0.5f, mB = 0.5f, tB = 0.5f;
    bool  dirty = true;

    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, a1 = 0, a2 = 0, a3 = 0;
    double makeup = 1.0;

    std::vector<double> z0, z1, z2;
};

/*=============================================================================
   Copyright (c) 2014-2018 Joel de Guzman. All rights reserved.

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <q/literals.hpp>
#include <q/sfx.hpp>
#include <q_io/audio_file.hpp>
#include <q/synth.hpp>
#include <q/envelope.hpp>

#include <vector>
#include <iostream>
#include <fstream>

#include "notes.hpp"
#include "pitch_follower.hpp"

namespace q = cycfi::q;
using namespace q::literals;
namespace audio_file = q::audio_file;

#define debug_signals

void process(
   std::string name
 , q::frequency lowest_freq
 , q::frequency highest_freq)
{
   ////////////////////////////////////////////////////////////////////////////
   // Read audio file

   auto src = audio_file::reader{"audio_files/" + name + ".wav"};
   std::uint32_t const sps = src.sps();

   std::vector<float> in(src.length());
   src.read(in);

   ////////////////////////////////////////////////////////////////////////////
   // Output
#ifdef debug_signals
   constexpr auto n_channels = 6;
#else
   constexpr auto n_channels = 2;
#endif
   std::vector<float> out(src.length() * n_channels);
   std::fill(out.begin(), out.end(), 0);

   auto max_val = *std::max_element(in.begin(), in.end(),
      [](auto a, auto b) { return std::abs(a) < std::abs(b); }
   );

   ////////////////////////////////////////////////////////////////////////////
   // Synthesizer

   // Our envelope
   auto env = q::envelope(
      q::envelope::config
      {
         10_ms     // attack rate
       , 200_ms    // decay rate
       , -6_dB     // sustain level
       , 50_s      // sustain rate
       , 250_ms    // release rate
      }
    , sps
   );

   auto f = q::phase(440_Hz, sps);     // Initial synth frequency
   auto ph = q::phase();               // Our phase accumulator
   auto pulse = q::pulse;              // Our pulse synth

   ////////////////////////////////////////////////////////////////////////////
   // Process

   q::pitch_follower::config  config(lowest_freq, highest_freq);
   q::pitch_follower          pf{config, sps};
   q::onset_detector          onset{ 0.6f, 100_ms, sps };
   bool                       is_attack = false;

   for (auto i = 0; i != in.size(); ++i)
   {
      auto pos = i * n_channels;
      auto ch1 = pos;      // input
      auto ch2 = pos+1;    // synth
      auto ch3 = pos+2;    // synth envelope state
      auto ch4 = pos+3;    // synth envelope
      auto ch5 = pos+4;    // onset
      auto ch6 = pos+5;    // input envelope

      auto s = in[i];

      // Pitch Detect
      if (pf(s))
      {
      }

      // onset
      auto o = onset(pf.audio());
      if (!is_attack && o != 0.0f)
      {
         env.trigger(o * 0.6);
         is_attack = env.state() == q::envelope::attack_state;
      }

      if (o == 0.0f)
         is_attack = false;

      out[ch1] = s * 1.0 / max_val;    // Input (normalized)

      auto synth_val = 0.0f;
      auto synth_env = env();

      if (o || (env.state() != q::envelope::note_off_state))
      {
         if (!o && !pf.gate())
            env.release();

         // Set frequency
         if (env.state() != q::envelope::release_state)
         {
            auto f_ = pf.frequency();
            if (f_ == 0.0f)
               f_ = pf.predict_frequency();
            if (f_ != 0.0f)
               f = q::phase(f_, sps);
         }

         auto pw = std::min(std::max<float>(synth_env*1.5f, 0.2f), 0.9f);
         pulse.width(pw);                       // Set pulse width
         synth_val = pulse(ph, f) * env();      // Synthesize
         ph += f;                               // Next
      }

#ifdef debug_signals
      out[ch3] = int(env.state()) / 5.0f;
      out[ch4] = env();
      out[ch5] = pf.gate() /*o*/ * 0.8f;
      out[ch6] = pf._cenv();
#endif

      out[ch2] = synth_val;
   }

   ////////////////////////////////////////////////////////////////////////////
   // Write to a wav file

   auto wav = audio_file::writer{
      "results/pitch_follower_" + name + ".wav", n_channels, sps
   };
   wav.write(out);
}

void process(std::string name, q::frequency lowest_freq)
{
   process(name, lowest_freq * 0.8, lowest_freq * 5);
}

int main()
{
   using namespace notes;

   // process("sin_440", d);
   // process("1-Low E", low_e);
   // process("2-Low E 2th", low_e);
   // process("3-A", a);
   // process("4-A 12th", a);
   // process("5-D", d);
   // process("6-D 12th", d);
   // process("7-G", g);
   // process("8-G 12th", g);
   // process("9-B", b);
   // process("10-B 12th", b);
   // process("11-High E", high_e);
   // process("12-High E 12th", high_e);

   process("Tapping D", d);
   process("Hammer-Pull High E", high_e);
   process("Bend-Slide G", g);

   process("SingleStaccato", g);
   process("GLines1", g);
   process("GLines2", g);
   process("GLines3", g);
   process("GStaccato", g);

   return 0;
}


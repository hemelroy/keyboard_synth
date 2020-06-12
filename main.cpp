#include <list>
#include <iostream>
#include <algorithm>
using namespace std;

#include "SoundMachine.h"

//Waveform constants
const int SINE_WAVE = 0;
const int SQUARE_WAVE = 1;
const int TRIANGLE_WAVE = 2;
const int SAWTOOTH_WAVE = 3;
const int NOISE = 4;
//Scale constant
const int SCALE_DEFAULT = 0;
const double TWO_TWELFTH_ROOT = 1.0594630943592952645618252949463; //2^(1/12)

// convert frequency to angular velocity
double hzToAngular(const double f)
{
	return f * 2.0 * PI;
}

struct note
{
	int id;		// Position in scale
	double on;	// Time note was activated
	double off;	// Time note was deactivated
	bool active;
	int channel;

	note()
	{
		id = 0;
		on = 0.0;
		off = 0.0;
		active = false;
		channel = 0;
	}
};

// oscillator function - generates waves
double oscillator(double t, double f, int nType = SINE_WAVE, double lowFreqOscHertz = 0.0, double lowFreqOscAmp = 0.0)
{
	double baseFreq = hzToAngular(f) * t + lowFreqOscAmp * f * (sin(hzToAngular(lowFreqOscHertz) * t));

	switch (nType)
	{
	case SINE_WAVE: // Sine wave bewteen -1 and +1
		return sin(baseFreq);

	case SQUARE_WAVE: // Square wave between -1 and +1
		return sin(baseFreq) > 0 ? 1.0 : -1.0;

	case TRIANGLE_WAVE: // triangle wave between -1 and +1
		return asin(sin(baseFreq)) * (2.0 / PI);

	case SAWTOOTH_WAVE: // saw wave 
	{
		double dOutput = 0.0;
		for (double n = 1.0; n < 50; n++)
			dOutput += (sin(n * baseFreq)) / n;
		return dOutput * (2.0 / PI);
	}

	case NOISE: // random noise
		return 2.0 * ((double)rand() / (double)RAND_MAX) - 1.0;

	default:
		return 0.0;
	}
}

// converts not on scale to corresponding frequency
double scale(const int nNoteID, const int nScaleID = SCALE_DEFAULT)
{
	switch (nScaleID)
	{
	case SCALE_DEFAULT: default:
		return 256 * pow(TWO_TWELFTH_ROOT, nNoteID); //256*[[2^(1/12)]^note] scales frequencies by 12 keyboard keys
	}
}


	
// Envelopes - used to control flow of sound (sustains, periods of rise and fall). Default is 0
struct envelope
{
	virtual double amplitude(const double time, const double timeOn, const double timeOff) = 0;
};

struct envelope_synth : public envelope //traditional attack, decay, sustain, release flow
{
	double attackTime, decayTime, releaseTime;
	double sustainAmplitude, startAmplitude;

	envelope_synth()
	{
		attackTime = 0.1;
		decayTime = 0.1;
		sustainAmplitude = 1.0;
		releaseTime = 0.2;
		startAmplitude = 1.0;
	}

	virtual double amplitude(const double time, const double timeOn, const double timeOff)
	{
		double amplitude = 0.0;
		double releaseAmplitude = 0.0;

		// If note is on, perform attack, decay, sustain
		if (timeOn > timeOff)
		{
			double lifeTime = time - timeOn;

			if (lifeTime <= attackTime)
				amplitude = (lifeTime / attackTime) * startAmplitude;

			if (lifeTime > attackTime && lifeTime <= (attackTime + decayTime))
				amplitude = ((lifeTime - attackTime) / decayTime) * (sustainAmplitude - startAmplitude) + startAmplitude;

			if (lifeTime > (attackTime + decayTime))
				amplitude = sustainAmplitude;
		}
		// If note is off, release
		else
		{
			double lifeTime = timeOff - timeOn;

			if (lifeTime <= attackTime)
				releaseAmplitude = (lifeTime / attackTime) * startAmplitude;

			if (lifeTime > attackTime && lifeTime <= (attackTime + decayTime))
				releaseAmplitude = ((lifeTime - attackTime) / decayTime) * (sustainAmplitude - startAmplitude) + startAmplitude;

			if (lifeTime > (attackTime + decayTime))
				releaseAmplitude = sustainAmplitude;

			amplitude = ((time - timeOff) / releaseTime) * (0.0 - releaseAmplitude) + releaseAmplitude;
		}

		// prevent negative amplitude
		if (amplitude <= 0.000)
			amplitude = 0.0;

		return amplitude;
	}
}; 

double envamp(const double time, envelope& env, const double timeOn, const double timeOff)
{
	return env.amplitude(time, timeOn, timeOff);
}

// Instrument Definitions
struct instrument
{
	double volume;
	envelope_synth env;
	virtual double createSound(const double time, note n, bool& isNoteFinished) = 0;
};

struct instrument_bell : public instrument
{
	instrument_bell()
	{
		env.attackTime = 0.01;
		env.decayTime = 1.0;
		env.sustainAmplitude = 0.0;
		env.releaseTime = 1.0;

		volume = 1.0;
	}

	virtual double createSound(const double time, note n, bool& isNoteFinished)
	{
		double amplitude = envamp(time, env, n.on, n.off);
		if (amplitude <= 0.0) 
			isNoteFinished = true;

		double sound = +1.00 * oscillator(n.on - time, scale(n.id + 12), SINE_WAVE, 5.0, 0.001)
			+ 0.50 * oscillator(n.on - time, scale(n.id + 48))
			+ 0.25 * oscillator(n.on - time, scale(n.id + 60));

		return amplitude * sound * volume;
	}

};

struct instrument_harmonica : public instrument
{
	instrument_harmonica()
	{
		env.attackTime = 0.05;
		env.decayTime = 1.0;
		env.sustainAmplitude = 0.95;
		env.releaseTime = 0.1;

		volume = 1.0;
	}

	virtual double createSound(const double time, note n, bool& isNoteFinished)
	{
		double amplitude = envamp(time, env, n.on, n.off);
		if (amplitude <= 0.0) 
			isNoteFinished = true;

		double sound = +1.00 * oscillator(n.on - time, scale(n.id), SQUARE_WAVE, 5.0, 0.001)
			+ 0.50 * oscillator(n.on - time, scale(n.id + 12), SQUARE_WAVE)
			+ 0.05 * oscillator(n.on - time, scale(n.id + 24), NOISE);

		return amplitude * sound * volume;
	}

};

struct instrument_harpsichord : public instrument
{
	instrument_harpsichord()
	{
		env.attackTime = 0.01;
		env.decayTime = 1.0;
		env.sustainAmplitude = 0.0;
		env.releaseTime = 1.0;

		volume = 1.0;
	}

	virtual double createSound(const double time, note n, bool& isNoteFinished)
	{
		double amplitude = envamp(time, env, n.on, n.off);
		if (amplitude <= 0.0)
			isNoteFinished = true;

		// Harpsicord
		double sound = +1.00 * oscillator(n.on - time, scale(n.id), SAWTOOTH_WAVE) + 0.2 * oscillator(n.on - time, scale(n.id+7), SAWTOOTH_WAVE);

		return amplitude * sound * volume;
	}

};

struct instrument_bass : public instrument
{
	instrument_bass()
	{
		env.attackTime = 0.01;
		env.decayTime = 0.5;
		env.sustainAmplitude = 0.0;
		env.releaseTime = 0.5;

		volume = 1.0;
	}

	virtual double createSound(const double time, note n, bool& isNoteFinished)
	{
		double amplitude = envamp(time, env, n.on, n.off);
		if (amplitude <= 0.0)
			isNoteFinished = true;

		double sound = 1.00 * oscillator(n.on - time, scale(n.id - 30), SAWTOOTH_WAVE)
			+ 0.8 * oscillator(n.on - time, scale(n.id - 30), SAWTOOTH_WAVE);	

		return amplitude * sound * volume;
	}
};


struct instrument_ocarina : public instrument
{
	instrument_ocarina()
	{
		env.attackTime = 0.1;
		env.decayTime = 0.0;
		env.sustainAmplitude = 1.0;
		env.releaseTime = 1.0;

		volume = 1.0;
	}

	virtual double createSound(const double time, note n, bool& isNoteFinished)
	{
		double amplitude = envamp(time, env, n.on, n.off);
		if (amplitude <= 0.0)
			isNoteFinished = true;

		//Ocarina
		double sound = 1.00 * oscillator(n.on - time, scale(n.id+4), SINE_WAVE)
					+ 0.8 * oscillator(n.on - time, scale(n.id+4), SINE_WAVE) + 0.08 * oscillator(n.on - time, scale(n.id), TRIANGLE_WAVE);

		return amplitude * sound * volume;
	}

};



vector<note> vecNotes;
mutex muxNotes;
instrument_bell instBell;
instrument_harmonica instHarm;
instrument_harpsichord instHpchord;
instrument_bass instBass; 
instrument_ocarina instOcar;



typedef bool(*lambda)(note const& item);
template<class T>
void safe_remove(T& v, lambda f)
{
	auto n = v.begin();
	while (n != v.end())
		if (!f(*n))
			n = v.erase(n);
		else
			++n;
}

// Function used by olcNoiseMaker to generate sound waves
// Returns amplitude (-1.0 to +1.0) as a function of time
double MakeNoise(int nChannel, double dTime)
{
	unique_lock<mutex> lm(muxNotes);
	double dMixedOutput = 0.0;

	for (auto& n : vecNotes)
	{
		bool bNoteFinished = false;
		double dSound = 0;

		int num = n.channel;
		switch (num)
		{
		case 1:
			dSound = instHarm.createSound(dTime, n, bNoteFinished) * 0.5;
			break;
		case 2:
			dSound = instBell.createSound(dTime, n, bNoteFinished);
			break;
		case 3:
			dSound = instHpchord.createSound(dTime, n, bNoteFinished) * 0.5;
			break;
		case 4:
			dSound = instBass.createSound(dTime, n, bNoteFinished);
			break;
		case 5:
			dSound = instOcar.createSound(dTime, n, bNoteFinished);
			break;
		default:
			dSound = 0;
			break;
		}
		
		dMixedOutput += dSound;

		if (bNoteFinished && n.off > n.on)
			n.active = false;
	}

	// Woah! Modern C++ Overload!!!
	safe_remove<vector<note>>(vecNotes, [](note const& item) { return item.active; });


	return dMixedOutput * 0.2;
}

void print_intro(void)
{
	wcout << endl << endl;

	wcout << endl <<
		"      #     #                             ###          #     #                           #####                                                                   " << endl <<
		"      #     # ###### #    # ###### #      ###  ####    ##   ## #    #  ####  #  ####    #     # #   # #    # ##### #    # ######  ####  # ###### ###### #####    " << endl <<
		"      #     # #      ##  ## #      #       #  #        # # # # #    # #      # #    #   #        # #  ##   #   #   #    # #      #      #     #  #      #    #   " << endl <<
		"      ####### #####  # ## # #####  #      #    ####    #  #  # #    #  ####  # #         #####    #   # #  #   #   ###### #####   ####  #    #   #####  #    #   " << endl <<
		"      #     # #      #    # #      #               #   #     # #    #      # # #              #   #   #  # #   #   #    # #           # #   #    #      #####    " << endl <<
		"      #     # #      #    # #      #          #    #   #     # #    # #    # # #    #   #     #   #   #   ##   #   #    # #      #    # #  #     #      #   #    " << endl <<
		"      #     # ###### #    # ###### ######      ####    #     #  ####   ####  #  ####     #####    #   #    #   #   #    # ######  ####  # ###### ###### #    #   " << endl << endl;

	
	wcout << "COMMAND MENU" << endl;

	// Display a keyboard
	wcout << endl <<
		"                |   |   |   |   |   | |   |   |   |   | |   | |   |   |   |" << endl <<
		"                |   | S |   |   | F | | G |   |   | J | | K | | L |   |   |" << endl <<
		"                |   |___|   |   |___| |___|   |   |___| |___| |___|   |   |__" << endl <<
		"                |     |     |     |     |     |     |     |     |     |     |" << endl <<
		"                |  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |" << endl <<
		"                |_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|" << endl << endl;

	wcout << "      #######################     INSTRUMENT SELECTION MENU:       #######################" << endl;
	wcout << "      #  q: Harmonica   |   w: Bells   |   e: Harpsichord   |   r: Bass   |   t: Ocarina #" << endl;
	wcout << "      ####################################################################################" << endl;
}

int main()
{
	// Get all sound hardware
	vector<wstring> devices = SoundMachine<short>::Enumerate();

	// Display findings
	for (auto d : devices) wcout << "Found Output Device: " << d << endl;
	wcout << "Using Device: " << devices[0] << endl;

	print_intro();
	wcout << "\nCurrent:  No instrument currently selected." << endl;

	// Create sound machine!!
	SoundMachine<short> sound(devices[0], 44100, 1, 8, 512);

	// Link noise function with sound machine
	sound.SetUserFunction(MakeNoise);

	char keyboard[129];
	memset(keyboard, ' ', 127);
	keyboard[128] = '\0';

	auto clock_old_time = chrono::high_resolution_clock::now();
	auto clock_real_time = chrono::high_resolution_clock::now();
	double dElapsedTime = 0.0;
	int selectorChan = 0;
	while (1)
	{
		if (GetAsyncKeyState('Q') & 0x8000)
		{
			selectorChan = 1;
			system("CLS");
			print_intro();
			wcout << endl << "Current: Harmonica " << endl;
			this_thread::sleep_for(100ms);
		}
		else if (GetAsyncKeyState('W') & 0x8000)
		{
			selectorChan = 2;
			system("CLS");
			print_intro();
			wcout << endl << "Current: Bells " << endl;
			this_thread::sleep_for(100ms);
		}
		else if (GetAsyncKeyState('E') & 0x8000)
		{
			selectorChan = 3;
			system("CLS");
			print_intro();
			wcout << endl << "Current: Harpsichord " << endl;
			this_thread::sleep_for(100ms);
		}
		else if (GetAsyncKeyState('R') & 0x8000)
		{
			selectorChan = 4;
			system("CLS");
			print_intro();
			wcout << endl << "Current: Bass " << endl;
			this_thread::sleep_for(100ms);
		}
		else if (GetAsyncKeyState('T') & 0x8000)
		{
			selectorChan = 5;
			system("CLS");
			print_intro();
			wcout << endl << "Current: Ocarina " << endl;
			this_thread::sleep_for(100ms);
		}

		for (int k = 0; k < 16; k++)
		{
			short nKeyState = GetAsyncKeyState((unsigned char)("ZSXCFVGBNJMK\xbcL\xbe\xbf"[k]));

			double dTimeNow = sound.GetTime();

			// Check if note already exists in currently playing notes
			muxNotes.lock();
			auto noteFound = find_if(vecNotes.begin(), vecNotes.end(), [&k](note const& item) { return item.id == k; });
			if (noteFound == vecNotes.end())
			{
				// Note not found in vector

				if (nKeyState & 0x8000)
				{
					// Key has been pressed so create a new note
					note n;
					n.id = k;
					n.on = dTimeNow;
					n.channel = selectorChan;
					n.active = true;

					// Add note to vector
					vecNotes.emplace_back(n);
				}
				else
				{
					// Note not in vector, but key has been released...
					// ...nothing to do
				}
			}
			else
			{
				// Note exists in vector
				if (nKeyState & 0x8000)
				{
					// Key is still held, so do nothing
					if (noteFound->off > noteFound->on)
					{
						// Key has been pressed again during release phase
						noteFound->on = dTimeNow;
						noteFound->active = true;
					}
				}
				else
				{
					// Key has been released, so switch off
					if (noteFound->off < noteFound->on)
					{
						noteFound->off = dTimeNow;
					}
				}
			}
			muxNotes.unlock();
		}
		//wcout << "\rNotes: " << vecNotes.size() << "    ";

		//this_thread::sleep_for(5ms);
	}

	return 0;
}
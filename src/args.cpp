/*
 * This file is part of sidplayfp, a console SID player.
 *
 * Copyright 2011-2021 Leandro Nini
 * Copyright 2000-2001 Simon White
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
// slightly modified by red M95 ;)

#include "player.h"

#include <iostream>

#include <cstring>
#include <climits>
#include <cstdlib>

#include "ini/types.h"

#include "sidlib_features.h"

#include "sidcxx11.h"

using std::cout;
using std::cerr;
using std::endl;

#ifdef HAVE_SIDPLAYFP_BUILDERS_HARDSID_H
# include <sidplayfp/builders/hardsid.h>
#endif

#ifdef HAVE_SIDPLAYFP_BUILDERS_EXSID_H
# include <sidplayfp/builders/exsid.h>
#endif

// Wide-chars are not yet supported here
#undef  SEPARATOR
#define SEPARATOR "/"

/**
 * Try load SID tune from HVSC_BASE
 */
bool ConsolePlayer::tryOpenTune(const char *hvscBase) {
    std::string newFileName(hvscBase);

    newFileName.append(SEPARATOR).append(m_filename);
    m_tune.load(newFileName.c_str());
    if (!m_tune.getStatus()) {
        return false;
    }

    m_filename.assign(newFileName);
    return true;
}

/**
 * Try load songlength DB from HVSC_BASE
 */
bool ConsolePlayer::tryOpenDatabase(const char *hvscBase, const char *suffix) {
    std::string newFileName(hvscBase);

    newFileName.append(SEPARATOR).append("DOCUMENTS").append(SEPARATOR).append("Songlengths.").append(suffix);
    return m_database.open(newFileName.c_str());
}

// Convert time from integer
bool parseTime(const char *str, uint_least32_t &time) {
    // Check for empty string
    if (*str == '\0')
        return false;

    uint_least32_t _time;
    uint_least32_t milliseconds = 0;

    char *sep = (char *) strstr(str, ":");
    if (!sep) { // User gave seconds
        _time = atoi (str);
    }
    else { // Read in MM:SS[.mmm] format
        int val;
        *sep = '\0';
        val  = atoi (str);
        if (val < 0 || val > 99)
            return false;
        _time = (uint_least32_t) val * 60;
        // parse milliseconds
        char *milli = (char *) strstr(sep+1, ".");
        if (milli) {
            char *start = milli + 1;
            char *end;
            milliseconds = strtol(start, &end, 10);
            switch (end - start) {
            case 1: milliseconds *= 100; break;
            case 2: milliseconds *= 10; break;
            case 3: break;
            default: return false;
            }

            if (milliseconds > 999)
                return false;

            *milli = '\0';
        }
        val = atoi (sep + 1);
        if (val < 0 || val > 59)
            return false;
        _time += (uint_least32_t) val;
    }

    time = _time * 1000 + milliseconds;
    return true;
}

bool parseAddress(const char *str, uint_least16_t &address) {
    if (*str == '\0')
        return false;

    long x = strtol(str, 0, 0);

    address = x;
    return true;
}

void displayDebugArgs() {
    std::ostream &out = cout;

    out << "Debug Options:" << endl
        << " --cpu-debug   Display CPU registers and disassemblies" << endl
        << " --delay=<num> Simulate C64 power-on delay (default: random)" << endl
        << " --noaudio     No audio output device" << endl
        << " --nosid       No SID emulation" << endl
        << " --none        No audio output device and no SID emulation" << endl;
}

// Parse command line arguments
int ConsolePlayer::args(int argc, const char *argv[]) {
    if (argc == 0) { // at least one argument required
        displayArgs();
        return -1;
    }

    // default arg options
    m_driver.output = OUT_SOUNDCARD;
    m_driver.file   = false;
    m_driver.info   = false;

    for (int i=0; i < 9; i++) {
        vMute[i] = false;
    }

    int     infile = 0;
    uint8_t i      = 0;
    bool    err    = false;

    // parse command line arguments
    while ((i < argc) && (argv[i] != nullptr)) {
        if ((argv[i][0] == '-') && (argv[i][1] != '\0')) {
            // help options
            if ((argv[i][1] == 'h') || !strcmp(&argv[i][1], "-help")) {
                displayArgs();
                return 0;
            }
            else if (!strcmp(&argv[i][1], "-help-dbg")) {
                displayDebugArgs();
                return 0;
            }

            else if (argv[i][1] == 'b') {
                if (!parseTime (&argv[i][2], m_timer.start))
                    err = true;
            }
            else if (strncmp (&argv[i][1], "ds", 2) == 0) { // Override sidTune and enable SID #2
                if (!parseAddress (&argv[i][3], m_engCfg.secondSidAddress))
                    err = true;
            }
#ifdef FEAT_THIRD_SID
            else if (strncmp (&argv[i][1], "ts", 2) == 0) { // Override sidTune and enable SID #3
                if (!parseAddress (&argv[i][3], m_engCfg.thirdSidAddress))
                    err = true;
            }
#endif
            else if (argv[i][1] == 'f') {
                if (argv[i][2] == '\0')
                    err = true;
                m_engCfg.frequency = (uint_least32_t) atoi (argv[i]+2);
            }

            // No filter options
            else if (strncmp (&argv[i][1], "nf", 2) == 0) {
                if (argv[i][3] == '\0')
                    m_filter.enabled = false;
            }

            // Track options
            else if (argv[i][1] == 'o') {
                if (argv[i][2] == 'l') {
                    m_track.loop   = true;
                    m_track.single = ((argv[i][3] == 's') ? true : false);
                    m_track.first  = atoi(&argv[i][((argv[i][3] == 's') ? 4 : 3)]);
                }
                else if (argv[i][2] == 's') {
                    m_track.loop   = ((argv[i][3] == 'l') ? true : false);
                    m_track.single = true;
                    m_track.first  = atoi(&argv[i][((argv[i][3] == 'l') ? 4 : 3)]);
                }
                else { // User didn't provide track number?
                    m_track.first = atoi(&argv[i][2]);
                }
            }

            // Channel muting
            else if (argv[i][1] == 'p') { // User didn't provide precision?
                if (argv[i][2] == '\0')
                    err = true;
                {
                    uint_least8_t precision = atoi(&argv[i][2]);
                    m_precision = ((precision <= 16) ? 16 : 32);
                }
            }

            else if (argv[i][1] == 'q') {
                m_quietLevel = ((argv[i][2] == '\0') ? 1 : atoi(&argv[i][2]));
            }

            else if (argv[i][1] == 't') {
                if (!parseTime (&argv[i][2], m_timer.length))
                    err = true;
                m_timer.valid = true;
            }

            // Resampling Options ----------
            else if (argv[i][1] == 'r') {
                if (argv[i][2] == 'i') {
                    m_engCfg.samplingMethod = SidConfig::INTERPOLATE;
                }
                else if (argv[i][2] == 'r') {
                    m_engCfg.samplingMethod = SidConfig::RESAMPLE_INTERPOLATE;
                }
                else {
                    err = true;
                }
                m_engCfg.fastSampling = ((argv[i][3] == 'f') ? true : false);
            }

            // SID model options
            else if (argv[i][1] == 's') { // Stereo playback
                m_channels = 2;
            }
            else if (argv[i][1] == 'm') { // Mono playback
                if (argv[i][2] == '\0') {
                    m_channels = 1;
                }
                else if (argv[i][2] == 'o') {
                    m_engCfg.defaultSidModel = SidConfig::MOS6581;
                }
                else if (argv[i][2] == 'n') {
                    m_engCfg.defaultSidModel = SidConfig::MOS8580;
                }
                else {
                    const int voice = atoi(&argv[i][2]);
                    if (voice > 0 && voice <= 9)
                        vMute[voice-1] = true;
                }
                m_engCfg.forceSidModel = ((argv[i][3] == 'f') ? true : false);
            }

#ifdef FEAT_DIGIBOOST
            else if (strcmp (&argv[i][1], "-digiboost") == 0) {
                m_engCfg.digiBoost = true;
            }
#endif
            // Video/Verbose Options
            else if (argv[i][1] == 'v') {
                if (argv[i][2] == '\0')
                    m_verboseLevel = 1;
                else if (argv[i][2] == 'f') {
                    m_engCfg.forceC64Model = true;
                }
                else if (argv[i][2] == 'n') {
                    m_engCfg.defaultC64Model = SidConfig::NTSC;
                }
                else if (argv[i][2] == 'p') {
                    m_engCfg.defaultC64Model = SidConfig::PAL;
                }
                else {
                    m_verboseLevel = atoi(&argv[i][2]);
                }
                m_engCfg.forceC64Model = ((argv[i][((argv[i][2] == 'f') ? 2 : 3)] == 'f') ? true : false);
            }
            else if (strncmp (&argv[i][1], "-delay=", 7) == 0) {
                m_engCfg.powerOnDelay = (uint_least16_t) atoi(&argv[i][8]);
            }

            // File format conversions
            else if (argv[i][1] == 'w' || strncmp (&argv[i][1], "-wav", 4) == 0) {
                m_driver.output = OUT_WAV;
                m_driver.file   = true;
                if (argv[i][((argv[i][1] == 'w') ? 2 : 5)] != '\0')
                    m_outfile = &argv[i][((argv[i][1] == 'w') ? 2 : 5)];
            }
            else if (strncmp (&argv[i][1], "-au", 3) == 0) {
                m_driver.output = OUT_AU;
                m_driver.file   = true;
                if (argv[i][4] != '\0')
                    m_outfile = &argv[i][4];
            }
            else if (strncmp (&argv[i][1], "-info", 5) == 0) {
                m_driver.info   = true;
            }
#ifdef HAVE_SIDPLAYFP_BUILDERS_RESIDFP_H
            else if (strcmp (&argv[i][1], "-residfp") == 0) {
                m_driver.sid    = EMU_RESIDFP;
            }
#endif // HAVE_SIDPLAYFP_BUILDERS_RESIDFP_H

#ifdef HAVE_SIDPLAYFP_BUILDERS_RESID_H
            else if (strcmp (&argv[i][1], "-resid") == 0) {
                m_driver.sid    = EMU_RESID;
            }
#endif // HAVE_SIDPLAYFP_BUILDERS_RESID_H

            // Hardware selection
#ifdef HAVE_SIDPLAYFP_BUILDERS_HARDSID_H
            else if (strcmp (&argv[i][1], "-hardsid") == 0) {
                m_driver.sid    = EMU_HARDSID;
                m_driver.output = OUT_NULL;
            }
#endif // HAVE_SIDPLAYFP_BUILDERS_HARDSID_H

#ifdef HAVE_SIDPLAYFP_BUILDERS_EXSID_H
            else if (strcmp (&argv[i][1], "-exsid") == 0) {
                m_driver.sid    = EMU_EXSID;
                m_driver.output = OUT_NULL;
            }
#endif // HAVE_SIDPLAYFP_BUILDERS_EXSID_H

            // These are for debug
            else if (strcmp (&argv[i][1], "-none") == 0) {
                m_driver.sid    = EMU_NONE;
                m_driver.output = OUT_NULL;
            }
            else if (strcmp (&argv[i][1], "-nosid") == 0) {
                m_driver.sid = EMU_NONE;
            }
            else if (strcmp (&argv[i][1], "-noaudio") == 0) {
                m_driver.output = OUT_NULL;
            }
            else if (strcmp (&argv[i][1], "-cpu-debug") == 0) {
                m_cpudebug = true;
            }

            else {
                err = true;
            }

        }
        else { // Reading file name
            if (infile == 0)
                infile = i;
            else
                err = true;
        }

        if (err) {
            displayArgs(argv[i]);
            return -1;
        }

        i++; // next index
    }

    const char* hvscBase = getenv("HVSC_BASE");

    // Load the tune
    m_filename = argv[infile];
    m_tune.load(m_filename.c_str());
    if (!m_tune.getStatus()) {
        std::string errorString(m_tune.statusString());

        // Try prepending HVSC_BASE
        if (!hvscBase || !tryOpenTune(hvscBase)) {
            displayError(errorString.c_str());
            return -1;
        }
    }

    // If filename specified we can only convert one song
    if (m_outfile != nullptr)
        m_track.single = true;

    // Can only loop if not creating audio files
    if (m_driver.output > OUT_SOUNDCARD)
        m_track.loop = false;

    // Check to see if we are trying to generate an audio file
    // whilst using a hardware emulation
    if (m_driver.file && (m_driver.sid >= EMU_HARDSID)) {
        displayError("ERROR: cannot generate audio files using hardware emulations");
        return -1;
    }

    if (m_driver.info && m_driver.file) {
        displayError("WARNING: metadata can be added only to wav files!");
    }

    // Select the desired track
    m_track.first    = m_tune.selectSong (m_track.first);
    m_track.selected = m_track.first;
    if (m_track.single)
        m_track.songs = 1;

    // If user provided no time then load songlength database
    // and set default lengths in case it's not found there.
    {
        if (m_driver.file && m_timer.valid && !m_timer.length) { // Time of 0 provided for wav generation?
            displayError ("ERROR: -t0 invalid in record mode");
            return -1;
        }
        if (!m_timer.valid) {
            m_timer.length = m_driver.file ? (m_iniCfg.sidplayfp()).recordLength : (m_iniCfg.sidplayfp()).playLength;

            bool dbOpened = false;
            if (hvscBase) {
                if (tryOpenDatabase(hvscBase, "md5")) {
                    dbOpened = true;
                    newSonglengthDB = true;
                }
                else if (tryOpenDatabase(hvscBase, "txt")) {
                    dbOpened = true;
                }
            }

            if (!dbOpened) {
                // Try load user configured songlength DB
                if ((m_iniCfg.sidplayfp()).database.length() != 0) {
                    // Try loading the database specificed by the user
#if defined(_WIN32) && defined(UNICODE)
# ifdef FEAT_DB_WCHAR_OPEN
                    const wchar_t *database = (m_iniCfg.sidplayfp()).database.c_str();
# else
                    char database[MAX_PATH];
                    const int ret = wcstombs(database, (m_iniCfg.sidplayfp()).database.c_str(), sizeof(database));
                    if (ret >= MAX_PATH)
                        database[0] = '\0';
# endif
#else
                    const char *database = (m_iniCfg.sidplayfp()).database.c_str();
#endif
                    if (!m_database.open(database)) {
                        displayError (m_database.error ());
                        return -1;
                    }

                    if ((m_iniCfg.sidplayfp()).database.find(TEXT(".md5")) != SID_STRING::npos)
                        newSonglengthDB = true;
                }
            }
        }
    }

#if HAVE_TSID == 1
    // Set TSIDs base directory
    if (!m_tsid.setBaseDir(true)) {
        displayError (m_tsid.getError());
        return -1;
    }
#endif

    // Configure engine with settings
    if (!m_engine.config (m_engCfg)) { // Config failed
        displayError(m_engine.error());
        return -1;
    }
    return 1;
}


void ConsolePlayer::displayArgs (const char *arg) {
    std::ostream &out = arg ? cerr : cout;

    if (arg)
        out << "Syntax error: " << arg << endl;
    else
        out << "Syntax: " << m_name << " [options] <file>" << endl;

    out << "Options:" << endl
        << " --help|-h   Display this screen" << endl
        << " --help-dbg  Debug help menu" << endl
        << " -b<num>     Set start time in [min:]sec[.milli] format" << endl
        << " -f<num>     Set frequency in Hz, default: " << SidConfig::DEFAULT_SAMPLING_FREQ << endl
        << " -ds<addr>   Set SID #2 address (e.g. -ds0xd420)" << endl
#ifdef FEAT_THIRD_SID
        << " -ts<addr>   Set SID #3 address (e.g. -ts0xd440)" << endl
#endif
        << " -nf         No SID filter emulation" << endl
        << " -o<l|s>     Looping and/or single track" << endl
        << " -o<num>     Start track (default: preset)" << endl
        << " -p<16|32>   Set format for file output (16 = signed 16 bit, 32 = 32 bit float, default: 16)" << endl
        << " -s          Force stereo output" << endl
        << " -m          Force mono output" << endl
        << " -m<num>     Mute voice <num> (e.g. -m1 -m2)" << endl
        << " -m<o|n>[f]  Set SID new/old chip model (default: old)," << endl
        << "             use 'f' to force the model" << endl
        << " -t<num>     Set play length in [min:]sec[.milli] format (0 = infinite)" << endl
        << " -<v|q>[x]   Verbose or quiet output. x is the optional level, default: 1" << endl
        << " -v[p|n][f]  Set VIC PAL/NTSC clock speed (default: defined by song)," << endl
        << "             use 'f' to force the clock by preventing speed fixing" << endl
#ifdef FEAT_DIGIBOOST
        << " --digiboost Enable digiboost for 8580 model" << endl
#endif
        << " -r[i|r][f]  Set resampling method (default: resample interpolate)," << endl
        << "             use 'f' to enable fast resampling (only for reSID)" << endl
        << " -w[name]    Create wav file (default: <datafile>[n].wav)" << endl
        << " --au[name]  Create au file (default: <datafile>[n].au)" << endl
        << " --info      Add metadata to wav file" << endl;

#ifdef HAVE_SIDPLAYFP_BUILDERS_RESIDFP_H
    out << " --residfp   use reSIDfp emulation (default)" << endl;
#endif

#ifdef HAVE_SIDPLAYFP_BUILDERS_RESID_H
    out << " --resid     use reSID emulation" << endl;
#endif

#ifdef HAVE_SIDPLAYFP_BUILDERS_HARDSID_H
    {
        HardSIDBuilder hs("");
        if (hs.availDevices ())
            out << " --hardsid   enable hardsid support" << endl;
    }
#endif
#ifdef HAVE_SIDPLAYFP_BUILDERS_EXSID_H
    {
        exSIDBuilder hs("");
        if (hs.availDevices ())
            out << " --exsid     enable exSID support" << endl;
    }
#endif
    out << endl
        << "Home page: " PACKAGE_URL << endl;
}

/*
 * This file is part of sidplayfp, a console SID player.
 *
 * Copyright 2011-2023 Leandro Nini
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

#include "codeConvert.h"

#include <cstring>
#include <ctype.h>

#include <iostream>
#include <iomanip>
#include <string>

using std::cout;
using std::cerr;
using std::endl;
using std::dec;
using std::hex;
using std::flush;
using std::setw;
using std::setfill;
using std::string;

#include <sidplayfp/SidInfo.h>
#include <sidplayfp/SidTuneInfo.h>

#ifdef FEAT_REGS_DUMP_SID
const char *noteName[] = {
    "---",
    "C-0", "C#0", "D-0", "D#0", "E-0", "F-0", "F#0", "G-0", "G#0", "A-0", "A#0", "B-0",
    "C-1", "C#1", "D-1", "D#1", "E-1", "F-1", "F#1", "G-1", "G#1", "A-1", "A#1", "B-1",
    "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2", "G#2", "A-2", "A#2", "B-2",
    "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "B-3",
    "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "B-4",
    "C-5", "C#5", "D-5", "D#5", "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "B-5",
    "C-6", "C#6", "D-6", "D#6", "E-6", "F-6", "F#6", "G-6", "G#6", "A-6", "A#6", "B-6",
    "C-7", "C#7", "D-7", "D#7", "E-7", "F-7", "F#7", "G-7", "G#7", "A-7", "A#7", "B-7",
};
#endif

const uint8_t tableWidth = 58;

const char info_file[]   = "Creating audio file: ";
const char info_file_q[] = "Creating audio file...";
const char info_quiet[]  = "Prev. [J] Pause [K] Next [L] Quit [Q] Go to [G]";
const char info_normal[] = "Prev. [J] Pause [K] Next [L] Quit [Q] Go to [G] Time: ";

const char esc[] = "\x1b[";

const char SID6581[] = "MOS6581";
const char SID8580[] = "CSG8580";

#ifdef FEAT_CONFIG_CIAMODEL
const char* getCia(SidConfig::cia_model_t model) {
    switch (model) {
    default:
    case SidConfig::MOS6526:
	return "MOS6526";
    case SidConfig::MOS8521:
	return "MOS8521";
    }
}
#endif

const char* getModel(SidTuneInfo::model_t model) {
    switch (model) {
    default:
    case SidTuneInfo::SIDMODEL_UNKNOWN:
        return "Unknown";
    case SidTuneInfo::SIDMODEL_6581:
        return SID6581;
    case SidTuneInfo::SIDMODEL_8580:
        return SID8580;
    case SidTuneInfo::SIDMODEL_ANY:
        return "Any";
    }
}

const char* getModel(SidConfig::sid_model_t model) {
    switch (model) {
    default:
    case SidConfig::MOS6581:
	return SID6581;
    case SidConfig::MOS8580:
        return SID8580;
    }
}

const char* getClock(SidTuneInfo::clock_t clock) {
    switch (clock) {
    default:
    case SidTuneInfo::CLOCK_UNKNOWN:
        return "Unknown";
    case SidTuneInfo::CLOCK_PAL:
        return "PAL";
    case SidTuneInfo::CLOCK_NTSC:
        return "NTSC";
    case SidTuneInfo::CLOCK_ANY:
        return "Any";
    }
}

string trimString(const char* str, uint8_t maxLen) {
    string data(str);
    // avoid too long file names
    if (data.length() > maxLen) {
        data.resize(maxLen - 3);
        data.append("...");
    }
    return data;
}

#ifdef FEAT_REGS_DUMP_SID
const char *ConsolePlayer::getNote(uint16_t freq) {
    if (freq) {
        int distance = 0xffff;
        for (int i=0; i < 96; ++i) {
            int d = abs(freq - m_freqTable[i]);
            if (d < distance)
                distance = d;
            else
                return noteName[i];
        }
        return noteName[96];
    }
    return noteName[0];
}
#endif

// Display console menu
void ConsolePlayer::menu () {
    if (m_quietLevel > 1) {
        if (m_driver.file)
            cerr << info_file_q;
        else
            cerr << info_quiet;
        return;
    }

    const SidInfo &info         = m_engine.info ();
    const SidTuneInfo *tuneInfo = m_tune.getInfo();

    if ((m_iniCfg.console()).ansi) {
        cerr << esc << "40m";  // Black background
        cerr << esc << "2J";   // Clear screen
        cerr << esc << "0;0H"; // Move cursor to 0,0
        cerr << esc << "?25l"; // hide the cursor
    }

    if (m_verboseLevel > 1) {
        cerr << "config file: ";
        SID_CERR << m_iniCfg.getFilename() << endl;
    }

    consoleTable (tableStart);
    consoleTable (tableMiddle);
    consoleColour(red, true);
    cerr << "    SID";
    consoleColour(green, true);
    cerr << "PLAY";
    consoleColour(blue, true);
    cerr << "FP";
    consoleColour(white, true);
    cerr << " - music player and C64 SID chip emulator" << endl;
    consoleTable (tableMiddle);
    consoleColour(white, false);

    {
        string version;
        version.reserve(tableWidth);
        version.append("Sidplayfp v" VERSION " - ").append(1, toupper(*info.name())).append(info.name() + 1).append(" v").append(info.version());
        cerr << setw(tableWidth/2 + version.length()/2) << version << endl;
    }

    const unsigned int n = tuneInfo->numberOfInfoStrings();
    if (n) {
        codeConvert codeset;

        consoleTable (tableSeparator);

        consoleTable (tableMiddle);
        consoleColour(cyan, true);
        cerr << " Title        : ";
        consoleColour(magenta, true);
        cerr << codeset.convert(tuneInfo->infoString(0)) << endl;
        if (n>1) {
            consoleTable (tableMiddle);
            consoleColour(cyan, true);
            cerr << " Composer(s)  : ";
            consoleColour(magenta, true);
            cerr << codeset.convert(tuneInfo->infoString(1)) << endl;
            consoleTable (tableMiddle);
            consoleColour(cyan, true);
            cerr << " Release      : ";
            consoleColour(magenta, true);
            cerr << codeset.convert(tuneInfo->infoString(2)) << endl;
        }
    }

    for (unsigned int i = 0; i < tuneInfo->numberOfCommentStrings(); i++) {
        consoleTable (tableMiddle);
        consoleColour(cyan, true);
        cerr << " Comment      : ";
        consoleColour(magenta, true);
        cerr << tuneInfo->commentString(i) << endl;
    }

    consoleTable (tableSeparator);

    if (m_verboseLevel) {
        consoleTable (tableMiddle);
        consoleColour(green, true);
        cerr << " File format  : ";
        consoleColour(white, true);
        cerr << tuneInfo->formatString() << endl;
        consoleTable (tableMiddle);
        consoleColour(green, true);
        cerr << " Filename(s)  : ";
        consoleColour(white, true);
        cerr << trimString(tuneInfo->dataFileName(), (tableWidth - 17)) << endl;

        // Second file is only sometimes present
        if (tuneInfo->infoFileName()) {
            consoleTable (tableMiddle);
            consoleColour(green, true);
            cerr << "              : ";
            consoleColour(white, true);
            cerr << tuneInfo->infoFileName() << endl;
        }
        consoleTable (tableMiddle);
        consoleColour(green, true);
        cerr << " Condition    : ";
        consoleColour(white, true);
        cerr << m_tune.statusString() << endl;

#if HAVE_TSID == 1
        if (!m_tsid) {
            consoleTable (tableMiddle);
            consoleColour(green, true);
            cerr << " TSID error   : ";
            consoleColour(white, true);
            cerr << m_tsid.getError () << endl;
        }
#endif // HAVE_TSID
    }

    consoleTable (tableMiddle);
    consoleColour(green, true);
    cerr << " Playlist     : ";
    consoleColour(white, true);

    { // This will be the format used for playlists
        int i = 1;
        if (!m_track.single) {
            i  = m_track.selected;
            i -= (m_track.first-1);
            if (i < 1)
                i += m_track.songs;
        }
        cerr << i << '/' << m_track.songs;
        cerr << " (tune " << tuneInfo->currentSong() << '/'
             << tuneInfo->songs() << '[' << tuneInfo->startSong() << "])";
    }

    if (m_track.loop)
        cerr << " - looping";

    cerr << endl;

    if (m_verboseLevel) {
        consoleTable (tableMiddle);
        consoleColour(green, true);
        cerr << " Song clock   : ";
        consoleColour(white, true);
        cerr << getClock(tuneInfo->clockSpeed()) << endl;
    }

    consoleTable (tableMiddle);
    consoleColour(green, true);
    cerr << " Duration     : ";
    consoleColour(white, true);
    if (m_timer.stop) {
        const uint_least32_t seconds = m_timer.stop / 1000;
        cerr << setw(2) << setfill('0') << ((seconds / 60) % 100)
             << ':' << setw(2) << setfill('0') << (seconds % 60);
#ifdef FEAT_NEW_SONLEGTH_DB
        cerr << '.' << setw(3) << m_timer.stop % 1000;
#endif
    }
    else if (m_timer.valid)
        cerr << "Infinite";
    else
        cerr << "Unknown";
    if (m_timer.start) {
        const uint_least32_t seconds = m_timer.start / 1000; // Show offset
        cerr << " -" << setw(2) << setfill('0') << ((seconds / 60) % 100)
             << ':' << setw(2) << setfill('0') << (seconds % 60);
    }

    cerr << endl;

    if (m_verboseLevel) {
        consoleTable(tableSeparator);
        consoleTable(tableMiddle);
        consoleColour(yellow, true);
        cerr << " Addresses    : " << hex;
        cerr.setf(std::ios::uppercase);
        consoleColour(white, false);

        // Display PSID Driver location
        cerr << "DRIVER = ";
        if (info.driverAddr() == 0)
            cerr << "NOT PRESENT";
        else {
            cerr << "$"  << setw(4) << setfill('0') << info.driverAddr();
            cerr << " - $" << setw(4) << setfill('0') << info.driverAddr() +
                (info.driverLength() - 1);
        }
        if (tuneInfo->playAddr() == 0xffff)
            cerr << ", SYS = $" << setw(4) << setfill('0') << tuneInfo->initAddr()
		         << ",";
        else
            cerr << ", INIT = $" << setw(4) << setfill('0') << tuneInfo->initAddr()
		         << ",";
        cerr << endl;
        consoleTable (tableMiddle);
        consoleColour(yellow, true);
        cerr << "              : ";
        consoleColour(white, false);
        cerr << "LOAD   = $" << setw(4) << setfill('0') << tuneInfo->loadAddr();
        cerr << " - $"       << setw(4) << setfill('0') << tuneInfo->loadAddr() +
             (tuneInfo->c64dataLen() - 1);
        if (tuneInfo->playAddr() != 0xffff)
            cerr << ", PLAY = $" << setw(4) << setfill('0') << tuneInfo->playAddr();
        cerr << dec << endl;

        consoleTable (tableMiddle);
        consoleColour(yellow, true);
        cerr << " SID details  : ";
        consoleColour(white, false);
        cerr << "SID #1 = $";
#ifdef FEAT_NEW_TUNEINFO_API
        cerr << hex << tuneInfo->sidChipBase(0) << dec;
        cerr << ", model: " << getModel(tuneInfo->sidModel(0));
#else
        cerr << hex << tuneInfo->sidChipBase1() << dec;
        cerr << ", model: " << getModel(tuneInfo->sidModel1());
#endif
        cerr << endl;
#ifdef FEAT_NEW_TUNEINFO_API
        if (tuneInfo->sidChips() > 1)
#else
        if (tuneInfo->isStereo())
#endif
        {
            consoleTable (tableMiddle);
            consoleColour(yellow, true);
            cerr << "              : ";
            consoleColour(white, false);
            cerr << "SID #2 = $";
#ifdef FEAT_NEW_TUNEINFO_API
            cerr << hex << tuneInfo->sidChipBase(1) << dec;
            cerr << ", model: " << getModel(tuneInfo->sidModel(1));
#else
            cerr << hex << tuneInfo->sidChipBase2() << dec;
            cerr << ", model: " << getModel(tuneInfo->sidModel2());
#endif
            cerr << endl;
#ifdef FEAT_NEW_TUNEINFO_API
            if (tuneInfo->sidChips() > 2) {
                consoleTable (tableMiddle);
                consoleColour(yellow, true);
                cerr << "              : ";
                consoleColour (white, false);
                cerr << "SID #3 = $" << hex << tuneInfo->sidChipBase(2) << dec;
                cerr << ", model: " << getModel(tuneInfo->sidModel(2));
                cerr << endl;
            }
#endif
        }

        consoleTable (tableSeparator);

#ifdef FEAT_CONFIG_CIAMODEL
	    consoleTable (tableMiddle);
	    consoleColour(yellow, true);
        cerr << " CIA model    : ";
	    consoleColour(white, false);
	    cerr << getCia(m_engCfg.ciaModel);
	    cerr << endl;
#endif
        consoleTable (tableMiddle);
        consoleColour(yellow, true);
        cerr << " Timing       : ";
        consoleColour(white, false);
        cerr << info.speedString() << endl;

        consoleTable (tableMiddle);
        consoleColour(yellow, true);
        cerr << " Channel mode : ";
        consoleColour(white, false);
        cerr << (info.channels() == 1 ? "Mono" : "Stereo") << endl;

        consoleTable (tableMiddle);
        consoleColour(yellow, true);
        cerr << " SID filter   : ";
        consoleColour(white, false);
        cerr << (m_filter.enabled ? "Enabled" : "Disabled") << endl;
#ifdef FEAT_DIGIBOOST
        consoleTable (tableMiddle);
        consoleColour(yellow, true);
        cerr << " DigiBoost    : ";
        consoleColour(white, false);
        cerr << (m_engCfg.digiBoost ? "Enabled" : "Disabled") << endl;
#endif
        consoleTable (tableMiddle);
        consoleColour(yellow, true);
        cerr << " SID model    : ";
        consoleColour(white, false);
        cerr << getModel(m_engCfg.defaultSidModel);
        cerr << (m_engCfg.forceSidModel ? " (forced)" : " (default)") << endl;

        if (m_verboseLevel > 1) {
            consoleTable (tableMiddle);
            consoleColour(yellow, true);
            cerr << " Delay        : ";
            consoleColour(white, false);
            cerr << info.powerOnDelay() << " cycles at power-on" << endl;
        }
    }

    const char* romDesc = info.kernalDesc();

    consoleTable (tableSeparator);

    consoleTable (tableMiddle);
    consoleColour(magenta, true);
    cerr << " KERNAL ROM   : ";
    if (strlen(romDesc) == 0) {
        consoleColour(red, false);
        cerr << "None - some tunes may not play!";
    }
    else {
        consoleColour(white, false);
        cerr << romDesc;
    }

    cerr << endl;

    romDesc = info.basicDesc();

    consoleTable (tableMiddle);
    consoleColour(magenta, true);
    cerr << " BASIC ROM    : ";
    if (strlen(romDesc) == 0) {
        consoleColour(red, false);
        cerr << "None - BASIC tunes unplayable!";
    }
    else {
        consoleColour(white, false);
        cerr << romDesc;
    }

    cerr << endl;

    romDesc = info.chargenDesc();

    consoleTable (tableMiddle);
    consoleColour(magenta, true);
    cerr << " Chargen ROM  : ";
    if (strlen(romDesc) == 0) {
        consoleColour(red, false);
        cerr << "None";
    }
    else {
        consoleColour(white, false);
        cerr << romDesc;
    }

    cerr << endl;

#ifdef FEAT_REGS_DUMP_SID
    if (m_quietLevel >= 1) {
	    consoleTable(tableEnd);
        cerr << info_quiet;
	return;
    }
    if (m_verboseLevel > 1) {
        consoleTable(tableSeparator);
        consoleTable(tableMiddle);
        int movLines = (m_verboseLevel > 2) ? (tuneInfo->sidChips() * 6) : (tuneInfo->sidChips() * 3);
	    cerr << "          Note  PW         Control          Waveform(s)" << endl;

        for (int i=0; i < movLines; i++) { // reserve space for SID status
            consoleTable(tableMiddle); cerr << '\n';
	    }
    }
#endif
    consoleTable(tableEnd);

    if (m_driver.file)
        cerr << info_file;
    else
        cerr << info_normal;

    // Get all the text to the screen so music playback is not disturbed.
    if (!m_quietLevel)
        cerr << "00:00";
    cerr << flush;
}

void ConsolePlayer::refreshRegDump() {
    if (m_quietLevel)
	return;
#ifdef FEAT_REGS_DUMP_SID
    if (m_verboseLevel > 1) {
        cerr.unsetf(std::ios::uppercase);
        const SidTuneInfo *tuneInfo = m_tune.getInfo();
        int movLines = (m_verboseLevel > 2) ? (tuneInfo->sidChips() * 6 + 1) : (tuneInfo->sidChips() * 3 + 1);

        // moves cursor enough for updating the viewer lines, depending on m_verboseLevel
        cerr << esc << movLines << "A\r"; 

        for (int j=0; j < tuneInfo->sidChips(); j++) {
            uint8_t* registers = m_registers[j];

            uint8_t  oldCtl[3];
                     oldCtl[0] = registers[0x04];
                     oldCtl[1] = registers[0x0b];
                     oldCtl[2] = registers[0x12];

            if (m_engine.getSidStatus(j, registers)) {
                oldCtl[0] ^= registers[0x04];
                oldCtl[1] ^= registers[0x0b];
                oldCtl[2] ^= registers[0x12];

	            for (int i=0; i < 3; i++) {
                    consoleTable (tableMiddle);
                    consoleColour(red, true);
                    cerr << " Voice " << (j * 3 + i+1) << ":" << hex;

                    consoleColour(white, true);
                    cerr << " " << getNote(registers[0x00 + i * 0x07] | ((registers[0x01 + i * 0x07] & 0x0f) << 8));

		            consoleColour(yellow, true);
                    cerr << "  $" << setw(3) << setfill('0')
		                 << (registers[0x02 + i * 0x07] | ((registers[0x03 + i * 0x07] & 0x0f) << 8)) << "  ";

                    {
                        uint8_t bitCnt[8];
	                            bitCnt[0] = 0x01;
	                            bitCnt[1] = 0x02;
    		                    bitCnt[2] = 0x04;
	           	                bitCnt[3] = 0x08;
                                bitCnt[4] = 0x10;
                                bitCnt[5] = 0x20;
                                bitCnt[6] = 0x40;
                                bitCnt[7] = 0x80;

                        for (int c=0; c < 8; c++) {
                            const char *CWOn[]  = {"GATE", "SYNC", "RING", "TEST", "TRI", "SAW", "PUL", "NOI",};
                            const char *CWOff[] = {"gate", "sync", "ring", "test", "___", "___", "___", "___",};
                            consoleColour((oldCtl[i] & bitCnt[c]) ? green : red, true);
                            cerr << ((registers[0x04 + i * 0x07] & bitCnt[c]) ? CWOn[c] : CWOff[c]) << " ";
    	                }
                    }
                    cerr << dec << '\n';
                }
	        }
            else {
                for (int i=0; i < 3; i++) {
                    consoleTable(tableMiddle); cerr << "???\n";
		        }
            }
	}
    if (m_verboseLevel <= 2)
        consoleTable(tableEnd);
    if (m_verboseLevel > 2) {
        for (int j=0; j < tuneInfo->sidChips(); j++) {
            uint8_t* registers = m_registers[j];

	        consoleTable(tableSeparator);
	        consoleTable(tableMiddle);
            cerr << " SID #" << (j + 1) << ": M. Vol.   Filters   F. Chn. F. Res.    F. Cut." << endl;
            consoleTable(tableMiddle);

            // binary volume meter, helps partially visualizing samples. yeah, i know it's a quite weird idea
	        consoleColour(red, true);
	        cerr << "          %";
            {
	            uint8_t bitCnt[4];
	                    bitCnt[0] = 0x08;
	              	    bitCnt[1] = 0x04;
                        bitCnt[2] = 0x02;
	                    bitCnt[3] = 0x01;

    	        for (int c=0; c < 4; c++) {
	                cerr << ((registers[0x18] & bitCnt[c]) ? "1" : "0");
	            }
            }

            cerr << "  ";
            {
                uint8_t bitCnt[4];
	                    bitCnt[0] = 0x10;
	                    bitCnt[1] = 0x20;
    		            bitCnt[2] = 0x40;
	           	        bitCnt[3] = 0x80;

	            for (int c=0; c < 4; c++) {
                    const char *filOn[]  = {"LP", "BP", "HP", "3O",};
                    const char *filOff[] = {"lp", "bp", "hp", "3o",};
                    cerr << ((registers[0x18] & bitCnt[c]) ? filOn[c] : filOff[c]) << " ";
	            }
            }

            cerr << "  ";
            {
		        uint8_t bitCnt[3];
		                bitCnt[0] = 0x01;
		                bitCnt[1] = 0x02;
		                bitCnt[2] = 0x04;

	            for (int c=0; c < 3; c++) {
                    const char *voice[] = {"1","2","3",};
	                cerr << ((registers[0x17] & bitCnt[c]) ? voice[c] : "-");
    	        }
            }

            // filter resonance display
            cerr << "    %";
            {
                uint8_t bitCnt[4];
	                    bitCnt[0] = 0x80;
	                    bitCnt[1] = 0x40;
    		            bitCnt[2] = 0x20;
	           	        bitCnt[3] = 0x10;

    	        for (int c=0; c < 4; c++) {
                    cerr << ((registers[0x17] & bitCnt[c]) ? "1" : "0");
	            }
            }

	        cerr << "  %";
            {
	            int bitCnt[11];
	                bitCnt[0]  = 0x400;
	                bitCnt[1]  = 0x200;
    	            bitCnt[2]  = 0x100;
	                bitCnt[3]  = 0x080;
	                bitCnt[4]  = 0x040;
		            bitCnt[5]  = 0x020;
		            bitCnt[6]  = 0x010;
		            bitCnt[7]  = 0x008;
	                bitCnt[8]  = 0x004;
		            bitCnt[9]  = 0x002;
		            bitCnt[10] = 0x001;

	            for (int c=0; c < 11; c++) {
	                cerr << ((registers[((c >= 2) ? 0x16 : 0x15)] & bitCnt[c]) ? "1" : "0");
                }
            }
	        cerr << dec << '\n';
	    }
        consoleTable(tableEnd);
    }
    else
#endif
        cerr << '\r';

    if (m_driver.file)
        cerr << info_file;
    else
        cerr << info_normal;

    cerr << flush;
    }
}

// Set colour of text on console
void ConsolePlayer::consoleColour (player_colour_t colour, bool bold) {
    if ((m_iniCfg.console()).ansi) {
        const char *mode = "";

        switch (colour) {
            case black:   mode = "30"; break;
            case red:     mode = "31"; break;
            case green:   mode = "32"; break;
            case yellow:  mode = "33"; break;
            case blue:    mode = "34"; break;
            case magenta: mode = "35"; break;
            case cyan:    mode = "36"; break;
            case white:   mode = "37"; break;
        }
        const char* bold_c = (bold) ? "1" : "0";

        cerr << esc << bold_c << ";40;" << mode << 'm';
    }
}

// Display menu outline
void ConsolePlayer::consoleTable (player_table_t table) {
    consoleColour(white, true);
    switch (table) {
    case tableStart:
        cerr << (m_iniCfg.console ()).topLeft << setw(tableWidth)
             << setfill((m_iniCfg.console ()).horizontal) << ""
             << (m_iniCfg.console ()).topRight;
        break;

    case tableMiddle:
        cerr << setw(tableWidth + 1) << setfill(' ') << ""
             << (m_iniCfg.console ()).vertical << '\r'
             << (m_iniCfg.console ()).vertical;
        return;

    case tableSeparator:
        cerr << (m_iniCfg.console ()).junctionRight << setw(tableWidth)
             << setfill((m_iniCfg.console ()).horizontal) << ""
             << (m_iniCfg.console ()).junctionLeft;
        break;

    case tableEnd:
        cerr << (m_iniCfg.console ()).bottomLeft << setw(tableWidth)
             << setfill((m_iniCfg.console ()).horizontal) << ""
             << (m_iniCfg.console ()).bottomRight;
        break;
    }
    // Move back to the beginning of the row and skip first char
    cerr << '\n';
}

// Restore ANSI console to defaults
void ConsolePlayer::consoleRestore () {
    if ((m_iniCfg.console()).ansi) {
	    cerr << esc << "?25h";
        cerr << esc << "0m";
    }
}

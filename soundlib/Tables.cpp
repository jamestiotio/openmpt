/*
 * Tables.cpp
 * ----------
 * Purpose: Effect, interpolation, data and other pre-calculated tables.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "Tables.h"
#include "Sndfile.h"

#include "Resampler.h"
#include "WindowedFIR.h"
#include <cmath>


OPENMPT_NAMESPACE_BEGIN


/////////////////////////////////////////////////////////////////////////////
// Note Name Tables

const mpt::uchar NoteNamesSharp[12][4] =
{
	UL_("C-"), UL_("C#"), UL_("D-"), UL_("D#"), UL_("E-"), UL_("F-"),
	UL_("F#"), UL_("G-"), UL_("G#"), UL_("A-"), UL_("A#"), UL_("B-")
};

const mpt::uchar NoteNamesFlat[12][4] =
{
	UL_("C-"), UL_("Db"), UL_("D-"), UL_("Eb"), UL_("E-"), UL_("F-"),
	UL_("Gb"), UL_("G-"), UL_("Ab"), UL_("A-"), UL_("Bb"), UL_("B-")
};


///////////////////////////////////////////////////////////
// File Formats Information (name, extension, etc)

struct ModFormatInfo
{
	const mpt::uchar *name;  // "ProTracker"
	const char *extension;   // "mod"
};

// Note: Formats with identical extensions must be grouped together.
static constexpr ModFormatInfo modFormatInfo[] =
{
	{ UL_("OpenMPT"),                            "mptm" },
	{ UL_("ProTracker"),                         "mod" },
	{ UL_("Scream Tracker 3"),                   "s3m" },
	{ UL_("FastTracker 2"),                      "xm" },
	{ UL_("Impulse Tracker"),                    "it" },

	{ UL_("Composer 669 / UNIS 669"),            "669" },
	{ UL_("ASYLUM Music Format"),                "amf" },
	{ UL_("DSMI Advanced Music Format"),         "amf" },
	{ UL_("Extreme's Tracker"),                  "ams" },
	{ UL_("Velvet Studio"),                      "ams" },
	{ UL_("CDFM / Composer 670"),                "c67" },
	{ UL_("DigiBooster Pro"),                    "dbm" },
	{ UL_("DigiBooster"),                        "digi" },
	{ UL_("X-Tracker"),                          "dmf" },
	{ UL_("DSMI Compact Advanced Music Format"), "dmf" },
	{ UL_("DSIK Format"),                        "dsm" },
	{ UL_("Digital Symphony"),                   "dsym" },
	{ UL_("Digital Tracker"),                    "dtm" },
	{ UL_("Farandole Composer"),                 "far" },
	{ UL_("FM Tracker"),                         "fmt" },
	{ UL_("Imago Orpheus"),                      "imf" },
	{ UL_("Ice Tracker"),                        "ice" },
#ifdef MPT_EXTERNAL_SAMPLES
	{ UL_("Impulse Tracker Project"),            "itp" },
#endif
	{ UL_("Galaxy Sound System"),                "j2b" },
	{ UL_("Soundtracker"),                       "m15" },
	{ UL_("Digitrakker"),                        "mdl" },
	{ UL_("OctaMED"),                            "med" },
	{ UL_("MultiMedia Sound"),                   "mms" },
	{ UL_("MadTracker 2"),                       "mt2" },
	{ UL_("MultiTracker"),                       "mtm" },
	{ UL_("Karl Morton Music Format"),           "mus" },
	{ UL_("NoiseTracker"),                       "nst" },
	{ UL_("Oktalyzer"),                          "okt" },
	{ UL_("Disorder Tracker 2"),                 "plm" },
	{ UL_("Epic Megagames MASI"),                "psm" },
	{ UL_("ProTracker"),                         "pt36" },
	{ UL_("PolyTracker"),                        "ptm" },
	{ UL_("SoundFX"),                            "sfx" },
	{ UL_("SoundFX"),                            "sfx2" },
	{ UL_("SoundTracker 2.6"),                   "st26" },
	{ UL_("Soundtracker"),                       "stk" },
	{ UL_("Scream Tracker 2"),                   "stm" },
	{ UL_("Scream Tracker Music Interface Kit"), "stx" },
	{ UL_("Soundtracker Pro II"),                "stp" },
	{ UL_("Symphonie"),                          "symmod"},
	{ UL_("Graoumf Tracker"),                    "gtk"},
	{ UL_("Graoumf Tracker 1 / 2"),              "gt2"},
	{ UL_("UltraTracker"),                       "ult" },
	{ UL_("Mod's Grave"),                        "wow" },
	// converted formats (no MODTYPE)
	{ UL_("General Digital Music"),              "gdm" },
	{ UL_("Un4seen MO3"),                        "mo3" },
	{ UL_("OggMod FastTracker 2"),               "oxm" },
#ifndef NO_ARCHIVE_SUPPORT
	// Compressed modules
	{ UL_("Compressed ProTracker"),              "mdz" },
	{ UL_("Compressed Module"),                  "mdr" },
	{ UL_("Compressed Scream Tracker 3"),        "s3z" },
	{ UL_("Compressed FastTracker 2"),           "xmz" },
	{ UL_("Compressed Impulse Tracker"),         "itz" },
	{ UL_("Compressed OpenMPT"),                 "mptmz" },
#endif
};


struct ModContainerInfo
{
	ModContainerType format;  // ModContainerType::XXX
	const mpt::uchar *name;   // "Unreal Music"
	const char *extension;    // "umx"
};

static constexpr ModContainerInfo modContainerInfo[] =
{
	// Container formats
	{ ModContainerType::UMX,     UL_("Unreal Music"),             "umx"   },
	{ ModContainerType::XPK,     UL_("XPK packed"),               "xpk"   },
	{ ModContainerType::PP20,    UL_("PowerPack PP20"),           "ppm"   },
	{ ModContainerType::MMCMP,   UL_("Music Module Compressor"),  "mmcmp" },
#ifdef MODPLUG_TRACKER
	{ ModContainerType::WAV,     UL_("Wave"),                     "wav"   },
	{ ModContainerType::UAX,     UL_("Unreal Sounds"),            "uax"   },
	{ ModContainerType::Generic, UL_("Generic Archive"),          ""      },
#endif
};


#ifdef MODPLUG_TRACKER
static constexpr ModFormatInfo otherFormatInfo[] =
{
	{ UL_("MIDI"), "mid" },
	{ UL_("MIDI"), "rmi" },
	{ UL_("MIDI"), "smf" }
};
#endif


std::vector<const char *> CSoundFile::GetSupportedExtensions(bool otherFormats)
{
	std::vector<const char *> exts;
	for(const auto &formatInfo : modFormatInfo)
	{
		// Avoid dupes in list
		const std::string_view ext = formatInfo.extension;
		if(ext.empty())
			continue;
		if(exts.empty() || ext != exts.back())
			exts.push_back(formatInfo.extension);
	}
	for(const auto &containerInfo : modContainerInfo)
	{
		// Avoid dupes in list
		const std::string_view ext = containerInfo.extension;
		if(ext.empty())
			continue;
		if(exts.empty() || ext != exts.back())
			exts.push_back(ext.data());
	}
#ifdef MODPLUG_TRACKER
	if(otherFormats)
	{
		for(const auto &formatInfo : otherFormatInfo)
		{
			exts.push_back(formatInfo.extension);
		}
	}
#else
	MPT_UNREFERENCED_PARAMETER(otherFormats);
#endif
	return exts;
}


static bool IsEqualExtension(std::string_view a, std::string_view b)
{
	if(a.length() != b.length())
	{
		return false;
	}
	return mpt::CompareNoCaseAscii(a, b) == 0;
}


bool CSoundFile::IsExtensionSupported(std::string_view ext)
{
	if(ext.length() == 0)
	{
		return false;
	}
	for(const auto &formatInfo : modFormatInfo)
	{
		if(IsEqualExtension(ext, formatInfo.extension))
		{
			return true;
		}
	}
	for(const auto &containerInfo : modContainerInfo)
	{
		if(IsEqualExtension(ext, containerInfo.extension))
		{
			return true;
		}
	}
	return false;
}


mpt::ustring CSoundFile::ModContainerTypeToString(ModContainerType containertype)
{
	for(const auto &containerInfo : modContainerInfo)
	{
		if(containerInfo.format == containertype)
		{
			return mpt::ToUnicode(mpt::Charset::UTF8, containerInfo.extension);
		}
	}
	return mpt::ustring();
}


mpt::ustring CSoundFile::ModContainerTypeToTracker(ModContainerType containertype)
{
	std::set<mpt::ustring> retvals;
	mpt::ustring retval;
	for(const auto &containerInfo : modContainerInfo)
	{
		if(containerInfo.format == containertype)
		{
			mpt::ustring name = containerInfo.name;
			if(retvals.insert(name).second)
			{
				if(!retval.empty())
				{
					retval += U_(" / ");
				}
				retval += name;
			}
		}
	}
	return retval;
}



///////////////////////////////////////////////////////////////////////

const uint8 ImpulseTrackerPortaVolCmd[16] =
{
	0x00, 0x01, 0x04, 0x08, 0x10, 0x20, 0x40, 0x60,
	0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// Period table for ProTracker octaves (1-7 in FastTracker 2, also used for file I/O):
const uint16 ProTrackerPeriodTable[7*12] =
{
	2*1712,2*1616,2*1524,2*1440,2*1356,2*1280,2*1208,2*1140,2*1076,2*1016,2*960,2*906,
	1712,1616,1524,1440,1356,1280,1208,1140,1076,1016,960,907,
	856,808,762,720,678,640,604,570,538,508,480,453,
	428,404,381,360,339,320,302,285,269,254,240,226,
	214,202,190,180,170,160,151,143,135,127,120,113,
	107,101,95,90,85,80,75,71,67,63,60,56,
	53,50,47,45,42,40,37,35,33,31,30,28
};


const uint16 ProTrackerTunedPeriods[16*12] =
{
	1712,1616,1524,1440,1356,1280,1208,1140,1076,1016,960,907,
	1700,1604,1514,1430,1348,1274,1202,1134,1070,1010,954,900,
	1688,1592,1504,1418,1340,1264,1194,1126,1064,1004,948,894,
	1676,1582,1492,1408,1330,1256,1184,1118,1056,996,940,888,
	1664,1570,1482,1398,1320,1246,1176,1110,1048,990,934,882,
	1652,1558,1472,1388,1310,1238,1168,1102,1040,982,926,874,
	1640,1548,1460,1378,1302,1228,1160,1094,1032,974,920,868,
	1628,1536,1450,1368,1292,1220,1150,1086,1026,968,914,862,
	1814,1712,1616,1524,1440,1356,1280,1208,1140,1076,1016,960,
	1800,1700,1604,1514,1430,1350,1272,1202,1134,1070,1010,954,
	1788,1688,1592,1504,1418,1340,1264,1194,1126,1064,1004,948,
	1774,1676,1582,1492,1408,1330,1256,1184,1118,1056,996,940,
	1762,1664,1570,1482,1398,1320,1246,1176,1110,1048,988,934,
	1750,1652,1558,1472,1388,1310,1238,1168,1102,1040,982,926,
	1736,1640,1548,1460,1378,1302,1228,1160,1094,1032,974,920,
	1724,1628,1536,1450,1368,1292,1220,1150,1086,1026,968,914
};

// Table for Invert Loop and Funk Repeat effects (EFx, .MOD only)
const uint8 ModEFxTable[16] =
{
	 0,  5,  6,  7,  8, 10, 11, 13,
	16, 19, 22, 26, 32, 43, 64, 128
};

// S3M C-4 periods
const uint16 FreqS3MTable[12] =
{
	1712,1616,1524,1440,1356,1280,1208,1140,1076,1016,960,907
};

// S3M FineTune frequencies
const uint16 S3MFineTuneTable[16] =
{
	7895,7941,7985,8046,8107,8169,8232,8280,
	8363,8413,8463,8529,8581,8651,8723,8757,	// 8363*2^((i-8)/(12*8))
};


// Sinus table
const int8 ModSinusTable[64] =
{
	0,12,25,37,49,60,71,81,90,98,106,112,117,122,125,126,
	127,126,125,122,117,112,106,98,90,81,71,60,49,37,25,12,
	0,-12,-25,-37,-49,-60,-71,-81,-90,-98,-106,-112,-117,-122,-125,-126,
	-127,-126,-125,-122,-117,-112,-106,-98,-90,-81,-71,-60,-49,-37,-25,-12
};

// Random wave table
const int8 ModRandomTable[64] =
{
	98,-127,-43,88,102,41,-65,-94,125,20,-71,-86,-70,-32,-16,-96,
	17,72,107,-5,116,-69,-62,-40,10,-61,65,109,-18,-38,-13,-76,
	-23,88,21,-94,8,106,21,-112,6,109,20,-88,-30,9,-127,118,
	42,-34,89,-4,-51,-72,21,-29,112,123,84,-101,-92,98,-54,-95
};

// Impulse Tracker sinus table (ITTECH.TXT)
const int8 ITSinusTable[256] =
{
	  0,  2,  3,  5,  6,  8,  9, 11, 12, 14, 16, 17, 19, 20, 22, 23,
	 24, 26, 27, 29, 30, 32, 33, 34, 36, 37, 38, 39, 41, 42, 43, 44,
	 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 56, 57, 58, 59,
	 59, 60, 60, 61, 61, 62, 62, 62, 63, 63, 63, 64, 64, 64, 64, 64,
	 64, 64, 64, 64, 64, 64, 63, 63, 63, 62, 62, 62, 61, 61, 60, 60,
	 59, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46,
	 45, 44, 43, 42, 41, 39, 38, 37, 36, 34, 33, 32, 30, 29, 27, 26,
 	 24, 23, 22, 20, 19, 17, 16, 14, 12, 11,  9,  8,  6,  5,  3,  2,
 	  0, -2, -3, -5, -6, -8, -9,-11,-12,-14,-16,-17,-19,-20,-22,-23,
	-24,-26,-27,-29,-30,-32,-33,-34,-36,-37,-38,-39,-41,-42,-43,-44,
	-45,-46,-47,-48,-49,-50,-51,-52,-53,-54,-55,-56,-56,-57,-58,-59,
	-59,-60,-60,-61,-61,-62,-62,-62,-63,-63,-63,-64,-64,-64,-64,-64,
	-64,-64,-64,-64,-64,-64,-63,-63,-63,-62,-62,-62,-61,-61,-60,-60,
	-59,-59,-58,-57,-56,-56,-55,-54,-53,-52,-51,-50,-49,-48,-47,-46,
	-45,-44,-43,-42,-41,-39,-38,-37,-36,-34,-33,-32,-30,-29,-27,-26,
	-24,-23,-22,-20,-19,-17,-16,-14,-12,-11, -9, -8, -6, -5, -3, -2,
};


// volume fade tables for Retrig Note:
const int8 retrigTable1[16] =
{ 0, 0, 0, 0, 0, 0, 10, 8, 0, 0, 0, 0, 0, 0, 24, 32 };

const int8 retrigTable2[16] =
{ 0, -1, -2, -4, -8, -16, 0, 0, 0, 1, 2, 4, 8, 16, 0, 0 };




const uint16 XMPeriodTable[104] =
{
	907,900,894,887,881,875,868,862,856,850,844,838,832,826,820,814,
	808,802,796,791,785,779,774,768,762,757,752,746,741,736,730,725,
	720,715,709,704,699,694,689,684,678,675,670,665,660,655,651,646,
	640,636,632,628,623,619,614,610,604,601,597,592,588,584,580,575,
	570,567,563,559,555,551,547,543,538,535,532,528,524,520,516,513,
	508,505,502,498,494,491,487,484,480,477,474,470,467,463,460,457,
	453,450,447,443,440,437,434,431
};


// floor(8363 * 64 * 2**(-n/768))
// 768 = 64 period steps for 12 notes
// Table is for highest possible octave
const uint32 XMLinearTable[768] =
{
	535232,534749,534266,533784,533303,532822,532341,531861,
	531381,530902,530423,529944,529466,528988,528511,528034,
	527558,527082,526607,526131,525657,525183,524709,524236,
	523763,523290,522818,522346,521875,521404,520934,520464,
	519994,519525,519057,518588,518121,517653,517186,516720,
	516253,515788,515322,514858,514393,513929,513465,513002,
	512539,512077,511615,511154,510692,510232,509771,509312,
	508852,508393,507934,507476,507018,506561,506104,505647,
	505191,504735,504280,503825,503371,502917,502463,502010,
	501557,501104,500652,500201,499749,499298,498848,498398,
	497948,497499,497050,496602,496154,495706,495259,494812,
	494366,493920,493474,493029,492585,492140,491696,491253,
	490809,490367,489924,489482,489041,488600,488159,487718,
	487278,486839,486400,485961,485522,485084,484647,484210,
	483773,483336,482900,482465,482029,481595,481160,480726,
	480292,479859,479426,478994,478562,478130,477699,477268,
	476837,476407,475977,475548,475119,474690,474262,473834,
	473407,472979,472553,472126,471701,471275,470850,470425,
	470001,469577,469153,468730,468307,467884,467462,467041,
	466619,466198,465778,465358,464938,464518,464099,463681,
	463262,462844,462427,462010,461593,461177,460760,460345,
	459930,459515,459100,458686,458272,457859,457446,457033,
	456621,456209,455797,455386,454975,454565,454155,453745,
	453336,452927,452518,452110,451702,451294,450887,450481,
	450074,449668,449262,448857,448452,448048,447644,447240,
	446836,446433,446030,445628,445226,444824,444423,444022,
	443622,443221,442821,442422,442023,441624,441226,440828,
	440430,440033,439636,439239,438843,438447,438051,437656,
	437261,436867,436473,436079,435686,435293,434900,434508,
	434116,433724,433333,432942,432551,432161,431771,431382,
	430992,430604,430215,429827,429439,429052,428665,428278,
	427892,427506,427120,426735,426350,425965,425581,425197,
	424813,424430,424047,423665,423283,422901,422519,422138,
	421757,421377,420997,420617,420237,419858,419479,419101,
	418723,418345,417968,417591,417214,416838,416462,416086,
	415711,415336,414961,414586,414212,413839,413465,413092,
	412720,412347,411975,411604,411232,410862,410491,410121,
	409751,409381,409012,408643,408274,407906,407538,407170,
	406803,406436,406069,405703,405337,404971,404606,404241,
	403876,403512,403148,402784,402421,402058,401695,401333,
	400970,400609,400247,399886,399525,399165,398805,398445,
	398086,397727,397368,397009,396651,396293,395936,395579,
	395222,394865,394509,394153,393798,393442,393087,392733,
	392378,392024,391671,391317,390964,390612,390259,389907,
	389556,389204,388853,388502,388152,387802,387452,387102,
	386753,386404,386056,385707,385359,385012,384664,384317,
	383971,383624,383278,382932,382587,382242,381897,381552,
	381208,380864,380521,380177,379834,379492,379149,378807,
	378466,378124,377783,377442,377102,376762,376422,376082,
	375743,375404,375065,374727,374389,374051,373714,373377,
	373040,372703,372367,372031,371695,371360,371025,370690,
	370356,370022,369688,369355,369021,368688,368356,368023,
	367691,367360,367028,366697,366366,366036,365706,365376,
	365046,364717,364388,364059,363731,363403,363075,362747,
	362420,362093,361766,361440,361114,360788,360463,360137,
	359813,359488,359164,358840,358516,358193,357869,357547,
	357224,356902,356580,356258,355937,355616,355295,354974,
	354654,354334,354014,353695,353376,353057,352739,352420,
	352103,351785,351468,351150,350834,350517,350201,349885,
	349569,349254,348939,348624,348310,347995,347682,347368,
	347055,346741,346429,346116,345804,345492,345180,344869,
	344558,344247,343936,343626,343316,343006,342697,342388,
	342079,341770,341462,341154,340846,340539,340231,339924,
	339618,339311,339005,338700,338394,338089,337784,337479,
	337175,336870,336566,336263,335959,335656,335354,335051,
	334749,334447,334145,333844,333542,333242,332941,332641,
	332341,332041,331741,331442,331143,330844,330546,330247,
	329950,329652,329355,329057,328761,328464,328168,327872,
	327576,327280,326985,326690,326395,326101,325807,325513,
	325219,324926,324633,324340,324047,323755,323463,323171,
	322879,322588,322297,322006,321716,321426,321136,320846,
	320557,320267,319978,319690,319401,319113,318825,318538,
	318250,317963,317676,317390,317103,316817,316532,316246,
	315961,315676,315391,315106,314822,314538,314254,313971,
	313688,313405,313122,312839,312557,312275,311994,311712,
	311431,311150,310869,310589,310309,310029,309749,309470,
	309190,308911,308633,308354,308076,307798,307521,307243,
	306966,306689,306412,306136,305860,305584,305308,305033,
	304758,304483,304208,303934,303659,303385,303112,302838,
	302565,302292,302019,301747,301475,301203,300931,300660,
	300388,300117,299847,299576,299306,299036,298766,298497,
	298227,297958,297689,297421,297153,296884,296617,296349,
	296082,295815,295548,295281,295015,294749,294483,294217,
	293952,293686,293421,293157,292892,292628,292364,292100,
	291837,291574,291311,291048,290785,290523,290261,289999,
	289737,289476,289215,288954,288693,288433,288173,287913,
	287653,287393,287134,286875,286616,286358,286099,285841,
	285583,285326,285068,284811,284554,284298,284041,283785,
	283529,283273,283017,282762,282507,282252,281998,281743,
	281489,281235,280981,280728,280475,280222,279969,279716,
	279464,279212,278960,278708,278457,278206,277955,277704,
	277453,277203,276953,276703,276453,276204,275955,275706,
	275457,275209,274960,274712,274465,274217,273970,273722,
	273476,273229,272982,272736,272490,272244,271999,271753,
	271508,271263,271018,270774,270530,270286,270042,269798,
	269555,269312,269069,268826,268583,268341,268099,267857
};


// round(65536 * 2**(n/768))
// 768 = 64 extra-fine finetune steps for 12 notes
// Table content is in 16.16 format
const uint32 FineLinearSlideUpTable[16] =
{
	65536, 65595, 65654, 65714,	65773, 65832, 65892, 65951,
	66011, 66071, 66130, 66190, 66250, 66309, 66369, 66429
};


// round(65536 * 2**(-n/768))
// 768 = 64 extra-fine finetune steps for 12 notes
// Table content is in 16.16 format
// Note that there are a few errors in this table (typos?), but well, this table comes straight from Impulse Tracker's source...
// Entry 0 (65535) should be 65536 (this value is unused and most likely stored this way so that it fits in a 16-bit integer)
// Entry 11 (64888) should be 64889 - rounding error?
// Entry 15 (64645) should be 64655 - typo?
const uint32 FineLinearSlideDownTable[16] =
{
	65535, 65477, 65418, 65359, 65300, 65241, 65182, 65123,
	65065, 65006, 64947, 64888, 64830, 64772, 64713, 64645
};


// round(65536 * 2**(n/192))
// 192 = 16 finetune steps for 12 notes
// Table content is in 16.16 format
const uint32 LinearSlideUpTable[256] =
{
	65536, 65773, 66011, 66250, 66489, 66730, 66971, 67213,
	67456, 67700, 67945, 68191, 68438, 68685, 68933, 69183,
	69433, 69684, 69936, 70189, 70443, 70698, 70953, 71210,
	71468, 71726, 71985, 72246, 72507, 72769, 73032, 73297,
	73562, 73828, 74095, 74363, 74632, 74902, 75172, 75444,
	75717, 75991, 76266, 76542, 76819, 77096, 77375, 77655,
	77936, 78218, 78501, 78785, 79069, 79355, 79642, 79930,
	80220, 80510, 80801, 81093, 81386, 81681, 81976, 82273,
	82570, 82869, 83169, 83469, 83771, 84074, 84378, 84683,
	84990, 85297, 85606, 85915, 86226, 86538, 86851, 87165,
	87480, 87796, 88114, 88433, 88752, 89073, 89396, 89719,
	90043, 90369, 90696, 91024, 91353, 91684, 92015, 92348,
	92682, 93017, 93354, 93691, 94030, 94370, 94711, 95054,
	95398, 95743, 96089, 96436, 96785, 97135, 97487, 97839,
	98193, 98548, 98905, 99262, 99621, 99982, 100343, 100706,
	101070, 101436, 101803, 102171, 102540, 102911, 103283, 103657,
	104032, 104408, 104786, 105165, 105545, 105927, 106310, 106694,
	107080, 107468, 107856, 108246, 108638, 109031, 109425, 109821,
	110218, 110617, 111017, 111418, 111821, 112226, 112631, 113039,
	113448, 113858, 114270, 114683, 115098, 115514, 115932, 116351,
	116772, 117194, 117618, 118043, 118470, 118899, 119329, 119760,
	120194, 120628, 121065, 121502, 121942, 122383, 122825, 123270,
	123715, 124163, 124612, 125063, 125515, 125969, 126425, 126882,
	127341, 127801, 128263, 128727, 129193, 129660, 130129, 130600,
	131072, 131546, 132022, 132499, 132978, 133459, 133942, 134427,
	134913, 135401, 135890, 136382, 136875, 137370, 137867, 138366,
	138866, 139368, 139872, 140378, 140886, 141395, 141907, 142420,
	142935, 143452, 143971, 144491, 145014, 145539, 146065, 146593,
	147123, 147655, 148189, 148725, 149263, 149803, 150345, 150889,
	151434, 151982, 152532, 153083, 153637, 154193, 154750, 155310,
	155872, 156435, 157001, 157569, 158139, 158711, 159285, 159861,
	160439, 161019, 161602, 162186, 162773, 163361, 163952, 164545
};


// round(65536 * 2**(-n/192))
// 192 = 16 finetune steps for 12 notes
// Table content is in 16.16 format
const uint32 LinearSlideDownTable[256] =
{
	65536, 65300, 65065, 64830, 64596, 64364, 64132, 63901,
	63670, 63441, 63212, 62984, 62757, 62531, 62306, 62081,
	61858, 61635, 61413, 61191, 60971, 60751, 60532, 60314,
	60097, 59880, 59664, 59449, 59235, 59022, 58809, 58597,
	58386, 58176, 57966, 57757, 57549, 57341, 57135, 56929,
	56724, 56519, 56316, 56113, 55911, 55709, 55508, 55308,
	55109, 54910, 54713, 54515, 54319, 54123, 53928, 53734,
	53540, 53347, 53155, 52963, 52773, 52582, 52393, 52204,
	52016, 51829, 51642, 51456, 51270, 51085, 50901, 50718,
	50535, 50353, 50172, 49991, 49811, 49631, 49452, 49274,
	49097, 48920, 48743, 48568, 48393, 48218, 48044, 47871,
	47699, 47527, 47356, 47185, 47015, 46846, 46677, 46509,
	46341, 46174, 46008, 45842, 45677, 45512, 45348, 45185,
	45022, 44859, 44698, 44537, 44376, 44216, 44057, 43898,
	43740, 43582, 43425, 43269, 43113, 42958, 42803, 42649,
	42495, 42342, 42189, 42037, 41886, 41735, 41584, 41434,
	41285, 41136, 40988, 40840, 40693, 40547, 40400, 40255,
	40110, 39965, 39821, 39678, 39535, 39392, 39250, 39109,
	38968, 38828, 38688, 38548, 38409, 38271, 38133, 37996,
	37859, 37722, 37586, 37451, 37316, 37181, 37047, 36914,
	36781, 36648, 36516, 36385, 36254, 36123, 35993, 35863,
	35734, 35605, 35477, 35349, 35221, 35095, 34968, 34842,
	34716, 34591, 34467, 34343, 34219, 34095, 33973, 33850,
	33728, 33607, 33486, 33365, 33245, 33125, 33005, 32887,
	32768, 32650, 32532, 32415, 32298, 32182, 32066, 31950,
	31835, 31720, 31606, 31492, 31379, 31266, 31153, 31041,
	30929, 30817, 30706, 30596, 30485, 30376, 30266, 30157,
	30048, 29940, 29832, 29725, 29618, 29511, 29405, 29299,
	29193, 29088, 28983, 28879, 28774, 28671, 28567, 28464,
	28362, 28260, 28158, 28056, 27955, 27855, 27754, 27654,
	27554, 27455, 27356, 27258, 27159, 27062, 26964, 26867,
	26770, 26674, 26577, 26482, 26386, 26291, 26196, 26102
};


// FT2's square root panning law LUT.
// Formula to generate this table: round(65536 * sqrt(n / 256))
const uint16 XMPanningTable[256] =
{
	0,     4096,  5793,  7094,  8192,  9159,  10033, 10837, 11585, 12288, 12953, 13585, 14189, 14768, 15326, 15864,
	16384, 16888, 17378, 17854, 18318, 18770, 19212, 19644, 20066, 20480, 20886, 21283, 21674, 22058, 22435, 22806,
	23170, 23530, 23884, 24232, 24576, 24915, 25249, 25580, 25905, 26227, 26545, 26859, 27170, 27477, 27780, 28081,
	28378, 28672, 28963, 29251, 29537, 29819, 30099, 30377, 30652, 30924, 31194, 31462, 31727, 31991, 32252, 32511,
	32768, 33023, 33276, 33527, 33776, 34024, 34270, 34514, 34756, 34996, 35235, 35472, 35708, 35942, 36175, 36406,
	36636, 36864, 37091, 37316, 37540, 37763, 37985, 38205, 38424, 38642, 38858, 39073, 39287, 39500, 39712, 39923,
	40132, 40341, 40548, 40755, 40960, 41164, 41368, 41570, 41771, 41972, 42171, 42369, 42567, 42763, 42959, 43154,
	43348, 43541, 43733, 43925, 44115, 44305, 44494, 44682, 44869, 45056, 45242, 45427, 45611, 45795, 45977, 46160,
	46341, 46522, 46702, 46881, 47059, 47237, 47415, 47591, 47767, 47942, 48117, 48291, 48465, 48637, 48809, 48981,
	49152, 49322, 49492, 49661, 49830, 49998, 50166, 50332, 50499, 50665, 50830, 50995, 51159, 51323, 51486, 51649,
	51811, 51972, 52134, 52294, 52454, 52614, 52773, 52932, 53090, 53248, 53405, 53562, 53719, 53874, 54030, 54185,
	54340, 54494, 54647, 54801, 54954, 55106, 55258, 55410, 55561, 55712, 55862, 56012, 56162, 56311, 56459, 56608,
	56756, 56903, 57051, 57198, 57344, 57490, 57636, 57781, 57926, 58071, 58215, 58359, 58503, 58646, 58789, 58931,
	59073, 59215, 59357, 59498, 59639, 59779, 59919, 60059, 60199, 60338, 60477, 60615, 60753, 60891, 61029, 61166,
	61303, 61440, 61576, 61712, 61848, 61984, 62119, 62254, 62388, 62523, 62657, 62790, 62924, 63057, 63190, 63323,
	63455, 63587, 63719, 63850, 63982, 64113, 64243, 64374, 64504, 64634, 64763, 64893, 65022, 65151, 65279, 65408,
};


// IT Vibrato -> OpenMPT/XM VibratoType
const uint8 AutoVibratoIT2XM[8] = { VIB_SINE, VIB_RAMP_DOWN, VIB_SQUARE, VIB_RANDOM, VIB_RAMP_UP, 0, 0, 0 };
// OpenMPT/XM VibratoType -> IT Vibrato
const uint8 AutoVibratoXM2IT[8] = { 0, 2, 4, 1, 3, 0, 0, 0 };

// Reversed sinc coefficients for 4x256 taps polyphase FIR resampling filter (SchismTracker's lutgen.c should generate a very similar table, but it's more precise)
const int16 CResampler::FastSincTable[256*4] =
{ // Cubic Spline
    0, 16384,     0,     0,   -31, 16383,    32,     0,   -63, 16381,    65,     0,   -93, 16378,   100,    -1,
 -124, 16374,   135,    -1,  -153, 16368,   172,    -3,  -183, 16361,   209,    -4,  -211, 16353,   247,    -5,
 -240, 16344,   287,    -7,  -268, 16334,   327,    -9,  -295, 16322,   368,   -12,  -322, 16310,   410,   -14,
 -348, 16296,   453,   -17,  -374, 16281,   497,   -20,  -400, 16265,   541,   -23,  -425, 16248,   587,   -26,
 -450, 16230,   634,   -30,  -474, 16210,   681,   -33,  -497, 16190,   729,   -37,  -521, 16168,   778,   -41,
 -543, 16145,   828,   -46,  -566, 16121,   878,   -50,  -588, 16097,   930,   -55,  -609, 16071,   982,   -60,
 -630, 16044,  1035,   -65,  -651, 16016,  1089,   -70,  -671, 15987,  1144,   -75,  -691, 15957,  1199,   -81,
 -710, 15926,  1255,   -87,  -729, 15894,  1312,   -93,  -748, 15861,  1370,   -99,  -766, 15827,  1428,  -105,
 -784, 15792,  1488,  -112,  -801, 15756,  1547,  -118,  -818, 15719,  1608,  -125,  -834, 15681,  1669,  -132,
 -850, 15642,  1731,  -139,  -866, 15602,  1794,  -146,  -881, 15561,  1857,  -153,  -896, 15520,  1921,  -161,
 -911, 15477,  1986,  -168,  -925, 15434,  2051,  -176,  -939, 15390,  2117,  -184,  -952, 15344,  2184,  -192,
 -965, 15298,  2251,  -200,  -978, 15251,  2319,  -208,  -990, 15204,  2387,  -216, -1002, 15155,  2456,  -225,
-1014, 15106,  2526,  -234, -1025, 15055,  2596,  -242, -1036, 15004,  2666,  -251, -1046, 14952,  2738,  -260,
-1056, 14899,  2810,  -269, -1066, 14846,  2882,  -278, -1075, 14792,  2955,  -287, -1084, 14737,  3028,  -296,
-1093, 14681,  3102,  -306, -1102, 14624,  3177,  -315, -1110, 14567,  3252,  -325, -1118, 14509,  3327,  -334,
-1125, 14450,  3403,  -344, -1132, 14390,  3480,  -354, -1139, 14330,  3556,  -364, -1145, 14269,  3634,  -374,
-1152, 14208,  3712,  -384, -1157, 14145,  3790,  -394, -1163, 14082,  3868,  -404, -1168, 14018,  3947,  -414,
-1173, 13954,  4027,  -424, -1178, 13889,  4107,  -434, -1182, 13823,  4187,  -445, -1186, 13757,  4268,  -455,
-1190, 13690,  4349,  -465, -1193, 13623,  4430,  -476, -1196, 13555,  4512,  -486, -1199, 13486,  4594,  -497,
-1202, 13417,  4676,  -507, -1204, 13347,  4759,  -518, -1206, 13276,  4842,  -528, -1208, 13205,  4926,  -539,
-1210, 13134,  5010,  -550, -1211, 13061,  5094,  -560, -1212, 12989,  5178,  -571, -1212, 12915,  5262,  -581,
-1213, 12842,  5347,  -592, -1213, 12767,  5432,  -603, -1213, 12693,  5518,  -613, -1213, 12617,  5603,  -624,
-1212, 12542,  5689,  -635, -1211, 12466,  5775,  -645, -1210, 12389,  5862,  -656, -1209, 12312,  5948,  -667,
-1208, 12234,  6035,  -677, -1206, 12156,  6122,  -688, -1204, 12078,  6209,  -698, -1202, 11999,  6296,  -709,
-1200, 11920,  6384,  -720, -1197, 11840,  6471,  -730, -1194, 11760,  6559,  -740, -1191, 11679,  6647,  -751,
-1188, 11598,  6735,  -761, -1184, 11517,  6823,  -772, -1181, 11436,  6911,  -782, -1177, 11354,  6999,  -792,
-1173, 11271,  7088,  -802, -1168, 11189,  7176,  -812, -1164, 11106,  7265,  -822, -1159, 11022,  7354,  -832,
-1155, 10939,  7442,  -842, -1150, 10855,  7531,  -852, -1144, 10771,  7620,  -862, -1139, 10686,  7709,  -872,
-1134, 10602,  7798,  -882, -1128, 10516,  7886,  -891, -1122, 10431,  7975,  -901, -1116, 10346,  8064,  -910,
-1110, 10260,  8153,  -919, -1103, 10174,  8242,  -929, -1097, 10088,  8331,  -938, -1090, 10001,  8420,  -947,
-1083,  9915,  8508,  -956, -1076,  9828,  8597,  -965, -1069,  9741,  8686,  -973, -1062,  9654,  8774,  -982,
-1054,  9566,  8863,  -991, -1047,  9479,  8951,  -999, -1039,  9391,  9039, -1007, -1031,  9303,  9127, -1015,
-1024,  9216,  9216, -1024, -1015,  9127,  9303, -1031, -1007,  9039,  9391, -1039,  -999,  8951,  9479, -1047,
 -991,  8863,  9566, -1054,  -982,  8774,  9654, -1062,  -973,  8686,  9741, -1069,  -965,  8597,  9828, -1076,
 -956,  8508,  9915, -1083,  -947,  8420, 10001, -1090,  -938,  8331, 10088, -1097,  -929,  8242, 10174, -1103,
 -919,  8153, 10260, -1110,  -910,  8064, 10346, -1116,  -901,  7975, 10431, -1122,  -891,  7886, 10516, -1128,
 -882,  7798, 10602, -1134,  -872,  7709, 10686, -1139,  -862,  7620, 10771, -1144,  -852,  7531, 10855, -1150,
 -842,  7442, 10939, -1155,  -832,  7354, 11022, -1159,  -822,  7265, 11106, -1164,  -812,  7176, 11189, -1168,
 -802,  7088, 11271, -1173,  -792,  6999, 11354, -1177,  -782,  6911, 11436, -1181,  -772,  6823, 11517, -1184,
 -761,  6735, 11598, -1188,  -751,  6647, 11679, -1191,  -740,  6559, 11760, -1194,  -730,  6471, 11840, -1197,
 -720,  6384, 11920, -1200,  -709,  6296, 11999, -1202,  -698,  6209, 12078, -1204,  -688,  6122, 12156, -1206,
 -677,  6035, 12234, -1208,  -667,  5948, 12312, -1209,  -656,  5862, 12389, -1210,  -645,  5775, 12466, -1211,
 -635,  5689, 12542, -1212,  -624,  5603, 12617, -1213,  -613,  5518, 12693, -1213,  -603,  5432, 12767, -1213,
 -592,  5347, 12842, -1213,  -581,  5262, 12915, -1212,  -571,  5178, 12989, -1212,  -560,  5094, 13061, -1211,
 -550,  5010, 13134, -1210,  -539,  4926, 13205, -1208,  -528,  4842, 13276, -1206,  -518,  4759, 13347, -1204,
 -507,  4676, 13417, -1202,  -497,  4594, 13486, -1199,  -486,  4512, 13555, -1196,  -476,  4430, 13623, -1193,
 -465,  4349, 13690, -1190,  -455,  4268, 13757, -1186,  -445,  4187, 13823, -1182,  -434,  4107, 13889, -1178,
 -424,  4027, 13954, -1173,  -414,  3947, 14018, -1168,  -404,  3868, 14082, -1163,  -394,  3790, 14145, -1157,
 -384,  3712, 14208, -1152,  -374,  3634, 14269, -1145,  -364,  3556, 14330, -1139,  -354,  3480, 14390, -1132,
 -344,  3403, 14450, -1125,  -334,  3327, 14509, -1118,  -325,  3252, 14567, -1110,  -315,  3177, 14624, -1102,
 -306,  3102, 14681, -1093,  -296,  3028, 14737, -1084,  -287,  2955, 14792, -1075,  -278,  2882, 14846, -1066,
 -269,  2810, 14899, -1056,  -260,  2738, 14952, -1046,  -251,  2666, 15004, -1036,  -242,  2596, 15055, -1025,
 -234,  2526, 15106, -1014,  -225,  2456, 15155, -1002,  -216,  2387, 15204,  -990,  -208,  2319, 15251,  -978,
 -200,  2251, 15298,  -965,  -192,  2184, 15344,  -952,  -184,  2117, 15390,  -939,  -176,  2051, 15434,  -925,
 -168,  1986, 15477,  -911,  -161,  1921, 15520,  -896,  -153,  1857, 15561,  -881,  -146,  1794, 15602,  -866,
 -139,  1731, 15642,  -850,  -132,  1669, 15681,  -834,  -125,  1608, 15719,  -818,  -118,  1547, 15756,  -801,
 -112,  1488, 15792,  -784,  -105,  1428, 15827,  -766,   -99,  1370, 15861,  -748,   -93,  1312, 15894,  -729,
  -87,  1255, 15926,  -710,   -81,  1199, 15957,  -691,   -75,  1144, 15987,  -671,   -70,  1089, 16016,  -651,
  -65,  1035, 16044,  -630,   -60,   982, 16071,  -609,   -55,   930, 16097,  -588,   -50,   878, 16121,  -566,
  -46,   828, 16145,  -543,   -41,   778, 16168,  -521,   -37,   729, 16190,  -497,   -33,   681, 16210,  -474,
  -30,   634, 16230,  -450,   -26,   587, 16248,  -425,   -23,   541, 16265,  -400,   -20,   497, 16281,  -374,
  -17,   453, 16296,  -348,   -14,   410, 16310,  -322,   -12,   368, 16322,  -295,    -9,   327, 16334,  -268,
   -7,   287, 16344,  -240,    -5,   247, 16353,  -211,    -4,   209, 16361,  -183,    -3,   172, 16368,  -153,
   -1,   135, 16374,  -124,    -1,   100, 16378,   -93,     0,    65, 16381,   -63,     0,    32, 16383,   -31,
};


/////////////////////////////////////////////////////////////////////////////////////////////


// Compute Bessel function Izero(y) using a series approximation
double Izero(double y)
{
	double s = 1, ds = 1, d = 0;
	do
	{
		d = d + 2;
		ds = ds * (y * y) / (d * d);
		s = s + ds;
	} while(ds > 1E-7 * s);
	return s;
}


static void getsinc(SINC_TYPE *psinc, double beta, double cutoff)
{
	if(cutoff >= 0.999)
	{
		// Avoid mixer overflows.
		// 1.0 itself does not make much sense.
		cutoff = 0.999;
	}
	const double izeroBeta = Izero(beta);
	const double kPi = 4.0 * std::atan(1.0) * cutoff;
	for(int isrc = 0; isrc < 8 * SINC_PHASES; isrc++)
	{
		double fsinc;
		int ix = 7 - (isrc & 7);
		ix = (ix * SINC_PHASES) + (isrc >> 3);
		if(ix == (4 * SINC_PHASES))
		{
			fsinc = 1.0;
		} else
		{
			const double x = (double)(ix - (4 * SINC_PHASES)) * (double)(1.0 / SINC_PHASES);
			const double xPi = x * kPi;
			fsinc = std::sin(xPi) * Izero(beta * std::sqrt(1 - x * x * (1.0 / 16.0))) / (izeroBeta * xPi); // Kaiser window
		}
		double coeff = fsinc * cutoff;
#ifdef MPT_INTMIXER
		*psinc++ = mpt::saturate_round<SINC_TYPE>(coeff * (1 << SINC_QUANTSHIFT));
#else
		*psinc++ = static_cast<SINC_TYPE>(coeff);
#endif
	}
}


#ifdef MODPLUG_TRACKER
bool CResampler::StaticTablesInitialized = false;
SINC_TYPE CResampler::gKaiserSinc[SINC_PHASES * 8];     // Upsampling
SINC_TYPE CResampler::gDownsample13x[SINC_PHASES * 8];  // Downsample 1.333x
SINC_TYPE CResampler::gDownsample2x[SINC_PHASES * 8];   // Downsample 2x
Paula::BlepTables CResampler::blepTables;               // Amiga BLEP resampler
#ifndef MPT_INTMIXER
mixsample_t CResampler::FastSincTablef[256 * 4];        // Cubic spline LUT
#endif // !defined(MPT_INTMIXER)
#endif // MODPLUG_TRACKER


void CResampler::InitFloatmixerTables()
{
#ifdef MPT_BUILD_FUZZER
	// Creating resampling tables can take a little while which we really should not spend
	// when fuzzing OpenMPT for crashes and hangs. This content of the tables is not really
	// relevant for any kind of possible crashes or hangs.
	return;
#endif // MPT_BUILD_FUZZER
#ifndef MPT_INTMIXER
	// Prepare fast sinc coefficients for floating point mixer
	for(std::size_t i = 0; i < std::size(FastSincTable); i++)
	{
		FastSincTablef[i] = static_cast<mixsample_t>(FastSincTable[i] * mixsample_t(1.0f / 16384.0f));
	}
#endif // !defined(MPT_INTMIXER)
}


void CResampler::InitializeTablesFromScratch(bool force)
{
	bool initParameterIndependentTables = false;
	if(force)
	{
		initParameterIndependentTables = true;
	}
#ifdef MODPLUG_TRACKER
	initParameterIndependentTables = !StaticTablesInitialized;
#endif  // MODPLUG_TRACKER

	MPT_MAYBE_CONSTANT_IF(initParameterIndependentTables)
	{
		InitFloatmixerTables();

		blepTables.InitTables();

		getsinc(gKaiserSinc, 9.6377, 0.97);
		getsinc(gDownsample13x, 8.5, 0.5);
		getsinc(gDownsample2x, 7.0, 0.425);

#ifdef MODPLUG_TRACKER
		StaticTablesInitialized = true;
#endif  // MODPLUG_TRACKER
	}

	if((m_OldSettings == m_Settings) && !force)
	{
		return;
	}

	m_WindowedFIR.InitTable(m_Settings.gdWFIRCutoff, m_Settings.gbWFIRType);

	m_OldSettings = m_Settings;
}


#ifdef MPT_RESAMPLER_TABLES_CACHED

static const CResampler & GetCachedResampler()
{
	static CResampler s_CachedResampler(true);
	return s_CachedResampler;
}


void CResampler::InitializeTablesFromCache()
{
	const CResampler & s_CachedResampler = GetCachedResampler();
	InitFloatmixerTables();
	std::copy(s_CachedResampler.gKaiserSinc, s_CachedResampler.gKaiserSinc + SINC_PHASES*8, gKaiserSinc);
	std::copy(s_CachedResampler.gDownsample13x, s_CachedResampler.gDownsample13x + SINC_PHASES*8, gDownsample13x);
	std::copy(s_CachedResampler.gDownsample2x, s_CachedResampler.gDownsample2x + SINC_PHASES*8, gDownsample2x);
	std::copy(s_CachedResampler.m_WindowedFIR.lut, s_CachedResampler.m_WindowedFIR.lut + WFIR_LUTLEN*WFIR_WIDTH, m_WindowedFIR.lut);
	blepTables = s_CachedResampler.blepTables;
}

#endif // MPT_RESAMPLER_TABLES_CACHED


#ifdef MPT_RESAMPLER_TABLES_CACHED_ONSTARTUP

struct ResampleCacheInitializer
{
	ResampleCacheInitializer()
	{
		GetCachedResampler();
	}
};
#if MPT_COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif // MPT_COMPILER_CLANG
static ResampleCacheInitializer g_ResamplerCachePrimer;
#if MPT_COMPILER_CLANG
#pragma clang diagnostic pop
#endif // MPT_COMPILER_CLANG

#endif // MPT_RESAMPLER_TABLES_CACHED_ONSTARTUP


OPENMPT_NAMESPACE_END

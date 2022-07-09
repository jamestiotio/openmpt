/*
 * mptTime.cpp
 * -----------
 * Purpose: Various time utility functions.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "mptTime.h"

#include "mptStringBuffer.h"

#if MPT_CXX_AT_LEAST(20) && !defined(MPT_LIBCXX_QUIRK_NO_CHRONO_DATE)
#include <chrono>
#endif

#if MPT_OS_WINDOWS
#include <windows.h>
#if defined(MODPLUG_TRACKER)
#include <mmsystem.h>
#endif
#endif


OPENMPT_NAMESPACE_BEGIN


namespace mpt
{
namespace Date
{

#if defined(MODPLUG_TRACKER)

#if MPT_OS_WINDOWS

namespace ANSI
{

uint64 Now()
{
	FILETIME filetime;
	GetSystemTimeAsFileTime(&filetime);
	return ((uint64)filetime.dwHighDateTime << 32 | filetime.dwLowDateTime);
}

mpt::ustring ToUString(uint64 time100ns)
{
	constexpr std::size_t bufsize = 256;

	mpt::ustring result;

	FILETIME filetime;
	SYSTEMTIME systime;
	filetime.dwHighDateTime = (DWORD)(((uint64)time100ns) >> 32);
	filetime.dwLowDateTime = (DWORD)((uint64)time100ns);
	FileTimeToSystemTime(&filetime, &systime);

	TCHAR buf[bufsize];

	GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, &systime, TEXT("yyyy-MM-dd"), buf, bufsize);
	result.append(mpt::ToUnicode(mpt::String::ReadWinBuf(buf)));

	result.append(U_(" "));

	GetTimeFormat(LOCALE_SYSTEM_DEFAULT, TIME_FORCE24HOURFORMAT, &systime, TEXT("HH:mm:ss"), buf, bufsize);
	result.append(mpt::ToUnicode(mpt::String::ReadWinBuf(buf)));

	result.append(U_("."));

	result.append(mpt::ufmt::dec0<3>((unsigned)systime.wMilliseconds));

	return result;

}

} // namespace ANSI

#endif // MPT_OS_WINDOWS

#endif // MODPLUG_TRACKER

#if MPT_CXX_BEFORE(20) || defined(MPT_LIBCXX_QUIRK_NO_CHRONO_DATE)

static int32 ToDaynum(int32 year, int32 month, int32 day)
{
	month = (month + 9) % 12;
	year = year - (month / 10);
	int32 daynum = year*365 + year/4 - year/100 + year/400 + (month*306 + 5)/10 + (day - 1);
	return daynum;
}

static void FromDaynum(int32 d, int32 & year, int32 & month, int32 & day)
{
	int64 g = d;
	int64 y,ddd,mi,mm,dd;

	y = (10000*g + 14780)/3652425;
	ddd = g - (365*y + y/4 - y/100 + y/400);
	if(ddd < 0)
	{
		y = y - 1;
		ddd = g - (365*y + y/4 - y/100 + y/400);
	}
	mi = (100*ddd + 52)/3060;
	mm = (mi + 2)%12 + 1;
	y = y + (mi + 2)/12;
	dd = ddd - (mi*306 + 5)/10 + 1;

	year = static_cast<int32>(y);
	month = static_cast<int32>(mm);
	day = static_cast<int32>(dd);
}

mpt::Date::Unix UnixFromUTC(UTC timeUtc)
{
	int32 daynum = ToDaynum(timeUtc.year, timeUtc.month, timeUtc.day);
	int64 seconds = static_cast<int64>(daynum - ToDaynum(1970, 1, 1)) * 24 * 60 * 60 + timeUtc.hours * 60 * 60 + timeUtc.minutes * 60 + timeUtc.seconds;
	return Unix{seconds};
}

mpt::Date::UTC UnixAsUTC(Unix tp)
{
	int64 tmp = tp.value;
	int64 seconds = tmp % 60; tmp /= 60;
	int64 minutes = tmp % 60; tmp /= 60;
	int64 hours   = tmp % 24; tmp /= 24;
	int32 year = 0, month = 0, day = 0;
	FromDaynum(static_cast<int32>(tmp) + ToDaynum(1970,1,1), year, month, day);
	mpt::Date::UTC result = {};
	result.year = year;
	result.month = month;
	result.day = day;
	result.hours = static_cast<int32>(hours);
	result.minutes = static_cast<int32>(minutes);
	result.seconds = seconds;
	return result;
}

#if defined(MODPLUG_TRACKER)

mpt::Date::Unix UnixFromLocal(Local timeLocal)
{
#if defined(MPT_FALLBACK_TIMEZONE_WINDOWS_HISTORIC)
	SYSTEMTIME sys_local{};
	sys_local.wYear = static_cast<uint16>(timeLocal.year);
	sys_local.wMonth = static_cast<uint16>(timeLocal.month);
	sys_local.wDay = static_cast<uint16>(timeLocal.day);
	sys_local.wHour = static_cast<uint16>(timeLocal.hours);
	sys_local.wMinute = static_cast<uint16>(timeLocal.minutes);
	sys_local.wSecond = static_cast<uint16>(timeLocal.seconds);
	sys_local.wMilliseconds = 0;
	DYNAMIC_TIME_ZONE_INFORMATION dtzi{};
	if(GetDynamicTimeZoneInformation(&dtzi) == TIME_ZONE_ID_INVALID) // WinVista
	{
		return mpt::Date::Unix{};
	}
	SYSTEMTIME sys_utc{};
	if(TzSpecificLocalTimeToSystemTimeEx(&dtzi, &sys_local, &sys_utc) == FALSE) // Win7/Win8
	{
		return mpt::Date::Unix{};
	}
	FILETIME ft{};
	if(SystemTimeToFileTime(&sys_utc, &ft) == FALSE) // Win 2000
	{
		return mpt::Date::Unix{};
	}
	ULARGE_INTEGER time_value{};
	time_value.LowPart = ft.dwLowDateTime;
	time_value.HighPart = ft.dwHighDateTime;
	return mpt::Date::UnixFromSeconds(static_cast<int64>((time_value.QuadPart - 116444736000000000LL) / 10000000LL));
#elif defined(MPT_FALLBACK_TIMEZONE_WINDOWS_CURRENT)
	SYSTEMTIME sys_local{};
	sys_local.wYear = static_cast<uint16>(timeLocal.year);
	sys_local.wMonth = static_cast<uint16>(timeLocal.month);
	sys_local.wDay = static_cast<uint16>(timeLocal.day);
	sys_local.wHour = static_cast<uint16>(timeLocal.hours);
	sys_local.wMinute = static_cast<uint16>(timeLocal.minutes);
	sys_local.wSecond = static_cast<uint16>(timeLocal.seconds);
	sys_local.wMilliseconds = 0;
	SYSTEMTIME sys_utc{};
	if(TzSpecificLocalTimeToSystemTime(NULL, &sys_local, &sys_utc) == FALSE) // WinXP
	{
		return mpt::Date::Unix{};
	}
	FILETIME ft{};
	if(SystemTimeToFileTime(&sys_utc, &ft) == FALSE) // Win 2000
	{
		return mpt::Date::Unix{};
	}
	ULARGE_INTEGER time_value{};
	time_value.LowPart = ft.dwLowDateTime;
	time_value.HighPart = ft.dwHighDateTime;
	return mpt::Date::UnixFromSeconds(static_cast<int64>((time_value.QuadPart - 116444736000000000LL) / 10000000LL));
#elif defined(MPT_FALLBACK_TIMEZONE_C)
	std::tm tmp{};
	tmp.tm_year = timeLocal.year - 1900;
	tmp.tm_mon = timeLocal.month - 1;
	tmp.tm_mday = timeLocal.day;
	tmp.tm_hour = timeLocal.hours;
	tmp.tm_min = timeLocal.minutes;
	tmp.tm_sec = static_cast<int>(timeLocal.seconds);
	return mpt::Date::UnixFromSeconds(static_cast<int64>(std::mktime(&tmp)));
#endif
}

mpt::Date::Local UnixAsLocal(Unix tp)
{
#if defined(MPT_FALLBACK_TIMEZONE_WINDOWS_HISTORIC)
	ULARGE_INTEGER time_value{};
	time_value.QuadPart = static_cast<int64>(mpt::Date::UnixAsSeconds(tp)) * 10000000LL + 116444736000000000LL;
	FILETIME ft{};
	ft.dwLowDateTime = time_value.LowPart;
	ft.dwHighDateTime = time_value.HighPart;
	SYSTEMTIME sys_utc{};
	if(FileTimeToSystemTime(&ft, &sys_utc) == FALSE) // WinXP
	{
		return mpt::Date::Local{};
	}
	DYNAMIC_TIME_ZONE_INFORMATION dtzi{};
	if(GetDynamicTimeZoneInformation(&dtzi) == TIME_ZONE_ID_INVALID) // WinVista
	{
		return mpt::Date::Local{};
	}
	SYSTEMTIME sys_local{};
	if(SystemTimeToTzSpecificLocalTimeEx(&dtzi, &sys_utc, &sys_local) == FALSE) // Win7/Win8
	{
		return mpt::Date::Local{};
	}
	mpt::Date::Local result{};
	result.year = sys_local.wYear;
	result.month = sys_local.wMonth;
	result.day = sys_local.wDay;
	result.hours = sys_local.wHour;
	result.minutes = sys_local.wMinute;
	result.seconds = sys_local.wSecond;
	return result;
#elif defined(MPT_FALLBACK_TIMEZONE_WINDOWS_CURRENT)
	ULARGE_INTEGER time_value{};
	time_value.QuadPart = static_cast<int64>(mpt::Date::UnixAsSeconds(tp)) * 10000000LL + 116444736000000000LL;
	FILETIME ft{};
	ft.dwLowDateTime = time_value.LowPart;
	ft.dwHighDateTime = time_value.HighPart;
	SYSTEMTIME sys_utc{};
	if(FileTimeToSystemTime(&ft, &sys_utc) == FALSE) // WinXP
	{
		return mpt::Date::Local{};
	}
	SYSTEMTIME sys_local{};
	if(SystemTimeToTzSpecificLocalTime(NULL, &sys_utc, &sys_local) == FALSE) // Win2000
	{
		return mpt::Date::Local{};
	}
	mpt::Date::Local result{};
	result.year = sys_local.wYear;
	result.month = sys_local.wMonth;
	result.day = sys_local.wDay;
	result.hours = sys_local.wHour;
	result.minutes = sys_local.wMinute;
	result.seconds = sys_local.wSecond;
	return result;
#elif defined(MPT_FALLBACK_TIMEZONE_C)
	std::time_t time_tp = static_cast<std::time_t>(mpt::Date::UnixAsSeconds(tp));
	std::tm *tmp = std::localtime(&time_tp);
	if(!tmp)
	{
		return mpt::Date::Local{};
	}
	std::tm local = *tmp;
	mpt::Date::Local result{};
	result.year = local.tm_year + 1900;
	result.month = local.tm_mon + 1;
	result.day = local.tm_mday;
	result.hours = local.tm_hour;
	result.minutes = local.tm_min;
	result.seconds = local.tm_sec;
	return result;
#endif
}

#endif // MODPLUG_TRACKER

#endif

template <LogicalTimezone TZ>
static mpt::ustring ToShortenedISO8601Impl(mpt::Date::Gregorian<TZ> date)
{
	mpt::ustring result;
	mpt::ustring tz;
	if constexpr(TZ == LogicalTimezone::Unspecified)
	{
		tz = U_("");
	} else if constexpr(TZ == LogicalTimezone::UTC)
	{
		tz = U_("Z");
	} else
	{
		tz = U_("");
	}
	if(date.year == 0)
	{
		return result;
	}
	result += mpt::ufmt::dec0<4>(date.year);
	result += U_("-") + mpt::ufmt::dec0<2>(date.month);
	result += U_("-") + mpt::ufmt::dec0<2>(date.day);
	if(date.hours == 0 && date.minutes == 0 && date.seconds)
	{
		return result;
	}
	result += U_("T");
	result += mpt::ufmt::dec0<2>(date.hours) + U_(":") + mpt::ufmt::dec0<2>(date.minutes);
	if(date.seconds == 0)
	{
		return result + tz;
	}
	result += U_(":") + mpt::ufmt::dec0<2>(date.seconds);
	result += tz;
	return result;
}

mpt::ustring ToShortenedISO8601(mpt::Date::AnyGregorian date)
{
	return ToShortenedISO8601Impl(date);
}

mpt::ustring ToShortenedISO8601(mpt::Date::UTC date)
{
	return ToShortenedISO8601Impl(date);
}

#ifdef MODPLUG_TRACKER
mpt::ustring ToShortenedISO8601(Local date)
{
	return ToShortenedISO8601Impl(date);
}
#endif // MODPLUG_TRACKER

} // namespace Date
} // namespace mpt



#ifdef MODPLUG_TRACKER

namespace Util
{

#if MPT_OS_WINDOWS

void MultimediaClock::Init()
{
	m_CurrentPeriod = 0;
}

void MultimediaClock::SetPeriod(uint32 ms)
{
	TIMECAPS caps = {};
	if(timeGetDevCaps(&caps, sizeof(caps)) != MMSYSERR_NOERROR)
	{
		return;
	}
	if((caps.wPeriodMax == 0) || (caps.wPeriodMin > caps.wPeriodMax))
	{
		return;
	}
	ms = std::clamp(mpt::saturate_cast<UINT>(ms), caps.wPeriodMin, caps.wPeriodMax);
	if(timeBeginPeriod(ms) != MMSYSERR_NOERROR)
	{
		return;
	}
	m_CurrentPeriod = ms;
}

void MultimediaClock::Cleanup()
{
	if(m_CurrentPeriod > 0)
	{
		if(timeEndPeriod(m_CurrentPeriod) != MMSYSERR_NOERROR)
		{
			// should not happen
			MPT_ASSERT_NOTREACHED();
		}
		m_CurrentPeriod = 0;
	}
}

MultimediaClock::MultimediaClock()
{
	Init();
}

MultimediaClock::MultimediaClock(uint32 ms)
{
	Init();
	SetResolution(ms);
}

MultimediaClock::~MultimediaClock()
{
	Cleanup();
}

uint32 MultimediaClock::SetResolution(uint32 ms)
{
	if(m_CurrentPeriod == ms)
	{
		return m_CurrentPeriod;
	}
	Cleanup();
	if(ms != 0)
	{
		SetPeriod(ms);
	}
	return GetResolution();
}

uint32 MultimediaClock::GetResolution() const
{
	return m_CurrentPeriod;
}

uint32 MultimediaClock::Now() const
{
	return timeGetTime();
}

uint64 MultimediaClock::NowNanoseconds() const
{
	return (uint64)timeGetTime() * (uint64)1000000;
}

#endif // MPT_OS_WINDOWS

} // namespace Util

#endif // MODPLUG_TRACKER


OPENMPT_NAMESPACE_END

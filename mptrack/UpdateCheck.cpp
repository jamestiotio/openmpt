/*
 * UpdateCheck.cpp
 * ---------------
 * Purpose: Class for easy software update check.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "UpdateCheck.h"
#include "BuildVariants.h"
#include "../common/version.h"
#include "../common/misc_util.h"
#include "../common/mptStringBuffer.h"
#include "Mptrack.h"
#include "TrackerSettings.h"
// Setup dialog stuff
#include "Mainfrm.h"
#include "../common/mptThread.h"
#include "../common/mptOSError.h"
#include "../misc/mptCrypto.h"
#include "HTTP.h"
#include "../misc/JSON.h"
#include "dlg_misc.h"
#include "../sounddev/SoundDeviceManager.h"
#include "ProgressDialog.h"
#include "Moddoc.h"


OPENMPT_NAMESPACE_BEGIN



namespace Update {

	struct windowsversion {
		uint64 version_major = 0;
		uint64 version_minor = 0;
		uint64 servicepack_major = 0;
		uint64 servicepack_minor = 0;
		uint64 build = 0;
		uint64 wine_major = 0;
		uint64 wine_minor = 0;
		uint64 wine_update = 0;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(windowsversion
		,version_major
		,version_minor
		,servicepack_major
		,servicepack_minor
		,build
		,wine_major
		,wine_minor
		,wine_update
	)

	struct autoupdate_installer {
		std::vector<mpt::ustring> arguments = {};
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(autoupdate_installer
		,arguments
	)

	struct autoupdate_archive {
		mpt::ustring subfolder = U_("");
		mpt::ustring restartbinary = U_("");
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(autoupdate_archive
		,subfolder
		,restartbinary
	)

	struct downloadinfo {
		mpt::ustring url = U_("");
		std::map<mpt::ustring, mpt::ustring> checksums = {};
		mpt::ustring filename = U_("");
		std::optional<autoupdate_installer> autoupdate_installer;
		std::optional<autoupdate_archive> autoupdate_archive;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(downloadinfo
		,url
		,checksums
		,filename
		,autoupdate_installer
		,autoupdate_archive
	)

	struct download {
		mpt::ustring url = U_("");
		mpt::ustring download_url = U_("");
		mpt::ustring type = U_("");
		bool can_autoupdate = false;
		mpt::ustring autoupdate_minversion = U_("");
		mpt::ustring os = U_("");
		std::optional<windowsversion> required_windows_version;
		std::map<mpt::ustring, bool> required_architectures = {};
		std::map<mpt::ustring, bool> supported_architectures = {};
		std::map<mpt::ustring, std::map<mpt::ustring, bool>> required_processor_features = {};
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(download
		,url
		,download_url
		,type
		,can_autoupdate
		,autoupdate_minversion
		,os
		,required_windows_version
		,required_architectures
		,supported_architectures
		,required_processor_features
	)

	struct versioninfo {
		mpt::ustring version = U_("");
		mpt::ustring date = U_("");
		mpt::ustring announcement_url = U_("");
		mpt::ustring changelog_url = U_("");
		std::map<mpt::ustring, download> downloads = {};
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(versioninfo
		,version
		,date
		,announcement_url
		,changelog_url
		,downloads
	)

	using versions = std::map<mpt::ustring, versioninfo>;

} // namespace Update


struct UpdateInfo {
	mpt::ustring version;
	mpt::ustring download;
	bool IsAvailable() const
	{
		return !version.empty();
	}
};

static bool IsCurrentArchitecture(const mpt::ustring &architecture)
{
	return mpt::Windows::Name(mpt::Windows::GetProcessArchitecture()) == architecture;
}

static bool IsArchitectureSupported(const mpt::ustring &architecture)
{
	const auto & architectures = mpt::Windows::GetSupportedProcessArchitectures(mpt::Windows::GetHostArchitecture());
	for(const auto & arch : architectures)
	{
		if(mpt::Windows::Name(arch) == architecture)
		{
			return true;
		}
	}
	return false;
}

static bool IsArchitectureFeatureSupported(const mpt::ustring &architecture, const mpt::ustring &feature)
{
	MPT_UNUSED_VARIABLE(architecture);
	#ifdef ENABLE_ASM
		if(feature == U_("")) return true;
		else if(feature == U_("lm")) return (CPU::GetAvailableFeatures() & CPU::feature::lm) != 0;
		else if(feature == U_("mmx")) return (CPU::GetAvailableFeatures() & CPU::feature::mmx) != 0;
		else if(feature == U_("sse")) return (CPU::GetAvailableFeatures() & CPU::feature::sse) != 0;
		else if(feature == U_("sse2")) return (CPU::GetAvailableFeatures() & CPU::feature::sse2) != 0;
		else if(feature == U_("sse3")) return (CPU::GetAvailableFeatures() & CPU::feature::sse3) != 0;
		else if(feature == U_("ssse3")) return (CPU::GetAvailableFeatures() & CPU::feature::ssse3) != 0;
		else if(feature == U_("sse4.1")) return (CPU::GetAvailableFeatures() & CPU::feature::sse4_1) != 0;
		else if(feature == U_("sse4.2")) return (CPU::GetAvailableFeatures() & CPU::feature::sse4_2) != 0;
		else if(feature == U_("avx")) return (CPU::GetAvailableFeatures() & CPU::feature::avx) != 0;
		else if(feature == U_("avx2")) return (CPU::GetAvailableFeatures() & CPU::feature::avx2) != 0;
		else return false;
	#else
		return true;
	#endif
}


static mpt::ustring GetChannelName(UpdateChannel channel)
{
	mpt::ustring channelName = U_("release");
	switch(channel)
	{
	case UpdateChannelDevelopment:
		channelName = U_("development");
		break;
	case UpdateChannelNext:
		channelName = U_("next");
		break;
	case UpdateChannelRelease:
		channelName = U_("release");
		break;
	default:
		channelName = U_("release");
		break;
	}
	return channelName;
}


static UpdateInfo GetBestDownload(const Update::versions &versions)
{
	
	UpdateInfo result;
	VersionWithRevision bestVersion = VersionWithRevision::Current();

	for(const auto & [versionname, versioninfo] : versions)
	{

		if(!VersionWithRevision::Parse(versioninfo.version).IsNewerThan(bestVersion))
		{
			continue;
		}

		mpt::ustring bestDownloadName;

		// check if version supports the current system
		bool is_supported = false;
		for(auto & [downloadname, download] : versioninfo.downloads)
		{

			// is it for windows?
			if(download.os != U_("windows") || !download.required_windows_version)
			{
				continue;
			}

			// can the installer run on the current system?
			bool download_supported = true;
			for(const auto & [architecture, required] : download.required_architectures)
			{
				if(!(required && IsArchitectureSupported(architecture)))
				{
					download_supported = false;
				}
			}

			// does the download run on current architecture?
			bool architecture_supported = false;
			for(const auto & [architecture, supported] : download.supported_architectures)
			{
				if(supported && IsCurrentArchitecture(architecture))
				{
					architecture_supported = true;
				}
			}
			if(!architecture_supported)
			{
				download_supported = false;
			}

			// does the current system have all required features?
			for(const auto & [architecture, features] : download.required_processor_features)
			{
				if(IsCurrentArchitecture(architecture))
				{
					for(const auto & [feature, required] : features)
					{
						if(!(required && IsArchitectureFeatureSupported(architecture, feature)))
						{
							download_supported = false;
						}
					}
				}
			}

			if(mpt::Windows::Version::Current().IsBefore(
					mpt::Windows::Version::System(mpt::saturate_cast<uint32>(download.required_windows_version->version_major), mpt::saturate_cast<uint32>(download.required_windows_version->version_minor)),
					mpt::Windows::Version::ServicePack(mpt::saturate_cast<uint16>(download.required_windows_version->servicepack_major), mpt::saturate_cast<uint16>(download.required_windows_version->servicepack_minor)),
					mpt::Windows::Version::Build(mpt::saturate_cast<uint32>(download.required_windows_version->build))
				))
			{
				download_supported = false;
			}

			if(mpt::Windows::IsWine() && theApp.GetWineVersion()->Version().IsValid())
			{
				if(theApp.GetWineVersion()->Version().IsBefore(mpt::Wine::Version(mpt::saturate_cast<uint8>(download.required_windows_version->wine_major), mpt::saturate_cast<uint8>(download.required_windows_version->wine_minor), mpt::saturate_cast<uint8>(download.required_windows_version->wine_update))))
				{
					download_supported = false;
				}
			}

			if(download_supported)
			{
				is_supported = true;
				if(theApp.IsInstallerMode() && download.type == U_("installer"))
				{
					bestDownloadName = downloadname;
				} else if(theApp.IsPortableMode() && download.type == U_("archive"))
				{
					bestDownloadName = downloadname;
				}
			}

		}

		if(is_supported)
		{
			bestVersion = VersionWithRevision::Parse(versioninfo.version);
			result.version = versionname;
			result.download = bestDownloadName;
		}

	}

	return result;

}


// Update notification dialog
class UpdateDialog : public CDialog
{
protected:
	const CString m_releaseVersion;
	const CString m_releaseDate;
	const CString m_releaseURL;
	const CString m_buttonText;
	CFont m_boldFont;

public:
	UpdateDialog(const CString &releaseVersion, const CString &releaseDate, const CString &releaseURL, const CString &buttonText = _T("&Update"))
		: CDialog(IDD_UPDATE)
		, m_releaseVersion(releaseVersion)
		, m_releaseDate(releaseDate)
		, m_releaseURL(releaseURL)
		, m_buttonText(buttonText)
	{ }

	BOOL OnInitDialog() override
	{
		CDialog::OnInitDialog();

		SetDlgItemText(IDOK, m_buttonText);

		CFont *font = GetDlgItem(IDC_VERSION2)->GetFont();
		LOGFONT lf;
		font->GetLogFont(&lf);
		lf.lfWeight = FW_BOLD;
		m_boldFont.CreateFontIndirect(&lf);
		GetDlgItem(IDC_VERSION2)->SetFont(&m_boldFont);

		SetDlgItemText(IDC_VERSION1, mpt::cfmt::val(VersionWithRevision::Current()));
		SetDlgItemText(IDC_VERSION2, m_releaseVersion);
		SetDlgItemText(IDC_DATE, m_releaseDate);
		SetDlgItemText(IDC_SYSLINK1, _T("More information about this build:\n<a href=\"") + m_releaseURL + _T("\">") + m_releaseURL + _T("</a>"));
		CheckDlgButton(IDC_CHECK1, (TrackerSettings::Instance().UpdateIgnoreVersion == m_releaseVersion) ? BST_CHECKED : BST_UNCHECKED);
		return FALSE;
	}

	void OnDestroy()
	{
		TrackerSettings::Instance().UpdateIgnoreVersion = IsDlgButtonChecked(IDC_CHECK1) != BST_UNCHECKED ? m_releaseVersion : CString();
		m_boldFont.DeleteObject();
		CDialog::OnDestroy();
	}

	void OnClickURL(NMHDR * /*pNMHDR*/, LRESULT * /*pResult*/)
	{
		CTrackApp::OpenURL(m_releaseURL);
	}

	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(UpdateDialog, CDialog)
	ON_NOTIFY(NM_CLICK, IDC_SYSLINK1, &UpdateDialog::OnClickURL)
	ON_WM_DESTROY()
END_MESSAGE_MAP()


mpt::ustring CUpdateCheck::GetStatisticsUserInformation(bool shortText)
{
	if(shortText)
	{
		return U_("A randomized user ID is sent together with basic system information."
			" This ID cannot be linked to you personally in any way."
			"\nOpenMPT will use this information to gather usage statistics and to plan system support for future OpenMPT versions.");
	} else
	{
		return U_(
			"When checking for updates, OpenMPT can additionally collect basic statistical information."
			" A randomized user ID is sent alongside the update check. This ID and the transmitted statistics cannot be linked to you personally in any way."
			" OpenMPT will use this information to gather usage statistics and to plan system support for future OpenMPT versions."
			"\nOpenMPT would collect the following statistical data points: OpenMPT version, Windows version, type of CPU, amount of RAM, sound device settings, configured update check frequency of OpenMPT.");
	}
}


#if MPT_UPDATE_LEGACY

mpt::ustring CUpdateCheck::GetDefaultChannelReleaseURL()
{
	return U_("https://update.openmpt.org/check/$VERSION/$GUID");
}

mpt::ustring CUpdateCheck::GetDefaultChannelNextURL()
{
	return U_("https://update.openmpt.org/check/next/$VERSION/$GUID");
}

mpt::ustring CUpdateCheck::GetDefaultChannelDevelopmentURL()
{
	return U_("https://update.openmpt.org/check/testing/$VERSION/$GUID");
}

#endif // MPT_UPDATE_LEGACY


std::vector<mpt::ustring> CUpdateCheck::GetDefaultUpdateSigningKeysRootAnchors()
{
	// IMPORTANT:
	// Signing keys are *NOT* stored on the same server as openmpt.org or the updates themselves,
	// because otherwise, a single compromised server could allow for rogue updates.
	return {
		U_("https://sagamusix.de/openmpt-update/"),
		U_("https://manx.datengang.de/openmpt/update/")
	};
}


mpt::ustring CUpdateCheck::GetDefaultAPIURL()
{
	return U_("https://update.openmpt.org/api/v3/");
}


std::atomic<int32> CUpdateCheck::s_InstanceCount(0);


int32 CUpdateCheck::GetNumCurrentRunningInstances()
{
	return s_InstanceCount.load();
}



bool CUpdateCheck::IsSuitableUpdateMoment()
{
	const auto documents = theApp.GetOpenDocuments();
	return std::all_of(documents.begin(), documents.end(), [](auto doc) { return !doc->IsModified(); });
}


// Start update check
void CUpdateCheck::StartUpdateCheckAsync(bool isAutoUpdate)
{
	bool loadPersisted = false;
	if(isAutoUpdate)
	{
		if(!TrackerSettings::Instance().UpdateEnabled)
		{
			return;
		}
		if(!IsSuitableUpdateMoment())
		{
			return;
		}
		int updateCheckPeriod = TrackerSettings::Instance().UpdateIntervalDays;
		if(updateCheckPeriod < 0)
		{
			return;
		}
		// Do we actually need to run the update check right now?
		const time_t now = time(nullptr);
		const time_t lastCheck = TrackerSettings::Instance().UpdateLastUpdateCheck.Get();
		// Check update interval. Note that we always check for updates when the system time had gone backwards (i.e. when the last update check supposedly happened in the future).
		const double secsSinceLastCheck = difftime(now, lastCheck);
		if(secsSinceLastCheck > 0.0 && secsSinceLastCheck < updateCheckPeriod * 86400.0)
		{
#if MPT_UPDATE_LEGACY
			if(!TrackerSettings::Instance().UpdateExperimentalNewAutoUpdate)
			{
				return;
			} else
#endif // MPT_UPDATE_LEGACY
			{
				loadPersisted = true;
			}
		}

		// Never ran update checks before, so we notify the user of automatic update checks.
		if(TrackerSettings::Instance().UpdateShowUpdateHint)
		{
			TrackerSettings::Instance().UpdateShowUpdateHint = false;
			const auto checkIntervalDays = TrackerSettings::Instance().UpdateIntervalDays.Get();
			CString msg = MPT_CFORMAT("OpenMPT would like to check for updates now, proceed?\n\nNote: In the future, OpenMPT will check for updates {}. If you do not want this, you can disable update checks in the setup.")
				(
					checkIntervalDays == 0 ? CString(_T("on every program start")) :
					checkIntervalDays == 1 ? CString(_T("every day")) :
					MPT_CFORMAT("every {} days")(checkIntervalDays)
				);
			if(Reporting::Confirm(msg, _T("OpenMPT Update")) == cnfNo)
			{
				TrackerSettings::Instance().UpdateLastUpdateCheck = mpt::Date::Unix(now);
				return;
			}
		}
	} else
	{
		if(!IsSuitableUpdateMoment())
		{
			Reporting::Notification(_T("Please save all modified modules before updating OpenMPT."), _T("OpenMPT Update"));
			return;
		}
		if(!TrackerSettings::Instance().UpdateEnabled)
		{
			if(Reporting::Confirm(_T("Update Check is disabled. Do you want to check anyway?"), _T("OpenMPT Update")) != cnfYes)
			{
				return;
			}
		}
	}
	TrackerSettings::Instance().UpdateShowUpdateHint = false;

	// ask if user wants to contribute system statistics
	if(!TrackerSettings::Instance().UpdateStatisticsConsentAsked)
	{
		const auto enableStatistics = Reporting::Confirm(
			U_("Do you want to contribute to OpenMPT by providing system statistics?\r\n\r\n") +
			mpt::String::Replace(CUpdateCheck::GetStatisticsUserInformation(false), U_("\n"), U_("\r\n")) + U_("\r\n\r\n") +
			MPT_UFORMAT("This option was previously {} on your system.\r\n")(TrackerSettings::Instance().UpdateStatistics ? U_("enabled") : U_("disabled")),
			false, !TrackerSettings::Instance().UpdateStatistics.Get());
		TrackerSettings::Instance().UpdateStatistics = (enableStatistics == ConfirmAnswer::cnfYes);
		TrackerSettings::Instance().UpdateStatisticsConsentAsked = true;
	}

	int32 expected = 0;
	if(!s_InstanceCount.compare_exchange_strong(expected, 1))
	{
		return;
	}

	CUpdateCheck::Context context;
	context.window = CMainFrame::GetMainFrame();
	context.msgStart = MPT_WM_APP_UPDATECHECK_START;
	context.msgProgress = MPT_WM_APP_UPDATECHECK_PROGRESS;
	context.msgCanceled = MPT_WM_APP_UPDATECHECK_CANCELED;
	context.msgFailure = MPT_WM_APP_UPDATECHECK_FAILURE;
	context.msgSuccess = MPT_WM_APP_UPDATECHECK_SUCCESS;
	context.autoUpdate = isAutoUpdate;
	context.loadPersisted = loadPersisted;
	context.statistics = GetStatisticsDataV3(CUpdateCheck::Settings());
	std::thread(CUpdateCheck::ThreadFunc(CUpdateCheck::Settings(), context)).detach();
}


CUpdateCheck::Settings::Settings()
	: periodDays(TrackerSettings::Instance().UpdateIntervalDays)
	, channel(static_cast<UpdateChannel>(TrackerSettings::Instance().UpdateChannel.Get()))
	, persistencePath(theApp.GetConfigPath())
#if MPT_UPDATE_LEGACY
	, modeLegacy(!TrackerSettings::Instance().UpdateExperimentalNewAutoUpdate)
	, channelReleaseURL(TrackerSettings::Instance().UpdateChannelReleaseURL)
	, channelNextURL(TrackerSettings::Instance().UpdateChannelNextURL)
	, channelDevelopmentURL(TrackerSettings::Instance().UpdateChannelDevelopmentURL)
#endif // MPT_UPDATE_LEGACY
	, apiURL(TrackerSettings::Instance().UpdateAPIURL)
	, sendStatistics(TrackerSettings::Instance().UpdateStatistics)
	, statisticsUUID(TrackerSettings::Instance().VersionInstallGUID)
{
}


CUpdateCheck::ThreadFunc::ThreadFunc(const CUpdateCheck::Settings &settings, const CUpdateCheck::Context &context)
	: settings(settings)
	, context(context)
{
	return;
}


void CUpdateCheck::ThreadFunc::operator () ()
{
	mpt::SetCurrentThreadPriority(context.autoUpdate ? mpt::ThreadPriorityLower : mpt::ThreadPriorityNormal);
	CheckForUpdate(settings, context);
}


std::string CUpdateCheck::GetStatisticsDataV3(const Settings &settings)
{
	JSON::value j;
	j["OpenMPT"]["Version"] = mpt::ufmt::val(Version::Current());
	j["OpenMPT"]["Architecture"] = mpt::Windows::Name(mpt::Windows::GetProcessArchitecture());
	j["Update"]["PeriodDays"] = settings.periodDays;
	j["System"]["Windows"]["Version"]["Name"] = mpt::Windows::Version::Current().GetName();
	j["System"]["Windows"]["Version"]["Major"] = mpt::Windows::Version::Current().GetSystem().Major;
	j["System"]["Windows"]["Version"]["Minor"] = mpt::Windows::Version::Current().GetSystem().Minor;
	j["System"]["Windows"]["ServicePack"]["Major"] = mpt::Windows::Version::Current().GetServicePack().Major;
	j["System"]["Windows"]["ServicePack"]["Minor"] = mpt::Windows::Version::Current().GetServicePack().Minor;
	j["System"]["Windows"]["Build"] = mpt::Windows::Version::Current().GetBuild();
	j["System"]["Windows"]["Architecture"] = mpt::Windows::Name(mpt::Windows::GetHostArchitecture());
	j["System"]["Windows"]["IsWine"] = mpt::Windows::IsWine();
	j["System"]["Windows"]["TypeRaw"] = MPT_FORMAT("0x{}")(mpt::fmt::HEX0<8>(mpt::Windows::Version::Current().GetTypeId()));
	std::vector<mpt::Windows::Architecture> architectures = mpt::Windows::GetSupportedProcessArchitectures(mpt::Windows::GetHostArchitecture());
	for(const auto & arch : architectures)
	{
		j["System"]["Windows"]["ProcessArchitectures"][mpt::ToCharset(mpt::Charset::UTF8, mpt::Windows::Name(arch))] = true;
	}
	j["System"]["Memory"] = mpt::Windows::GetSystemMemorySize() / 1024 / 1024;  // MB
	j["System"]["Threads"] = std::thread::hardware_concurrency();
	if(mpt::Windows::IsWine())
	{
		mpt::Wine::VersionContext v;
		j["System"]["Windows"]["Wine"]["Version"]["Raw"] = v.RawVersion();
		if(v.Version().IsValid())
		{
			j["System"]["Windows"]["Wine"]["Version"]["Major"] = v.Version().GetMajor();
			j["System"]["Windows"]["Wine"]["Version"]["Minor"] = v.Version().GetMinor();
			j["System"]["Windows"]["Wine"]["Version"]["Update"] = v.Version().GetUpdate();
		}
		j["System"]["Windows"]["Wine"]["HostSysName"] = v.RawHostSysName();
	}
	const SoundDevice::Identifier deviceIdentifier = TrackerSettings::Instance().GetSoundDeviceIdentifier();
	const SoundDevice::Info deviceInfo = theApp.GetSoundDevicesManager()->FindDeviceInfo(deviceIdentifier);
	const SoundDevice::Settings deviceSettings = TrackerSettings::Instance().GetSoundDeviceSettings(deviceIdentifier);
	j["OpenMPT"]["SoundDevice"]["Type"] = deviceInfo.type;
	j["OpenMPT"]["SoundDevice"]["Name"] = deviceInfo.name;
	j["OpenMPT"]["SoundDevice"]["Settings"]["Samplerate"] = deviceSettings.Samplerate;
	j["OpenMPT"]["SoundDevice"]["Settings"]["Latency"] = deviceSettings.Latency;
	j["OpenMPT"]["SoundDevice"]["Settings"]["UpdateInterval"] = deviceSettings.UpdateInterval;
	j["OpenMPT"]["SoundDevice"]["Settings"]["Channels"] = deviceSettings.Channels.GetNumHostChannels();
	j["OpenMPT"]["SoundDevice"]["Settings"]["BoostThreadPriority"] = deviceSettings.BoostThreadPriority;
	j["OpenMPT"]["SoundDevice"]["Settings"]["ExclusiveMode"] = deviceSettings.ExclusiveMode;
	j["OpenMPT"]["SoundDevice"]["Settings"]["UseHardwareTiming"] = deviceSettings.UseHardwareTiming;
	j["OpenMPT"]["SoundDevice"]["Settings"]["KeepDeviceRunning"] = deviceSettings.KeepDeviceRunning;
	#ifdef ENABLE_ASM
		j["OpenMPT"]["cpuid"] = ((CPU::GetAvailableFeatures() & CPU::feature::cpuid) != 0);
		j["System"]["Processor"]["Vendor"] = std::string(mpt::String::ReadAutoBuf(CPU::ProcVendorID));
		j["System"]["Processor"]["Brand"] = std::string(mpt::String::ReadAutoBuf(CPU::ProcBrandID));
		j["System"]["Processor"]["CpuidRaw"] = mpt::fmt::hex0<8>(CPU::ProcRawCPUID);
		j["System"]["Processor"]["Id"]["Family"] = CPU::ProcFamily;
		j["System"]["Processor"]["Id"]["Model"] = CPU::ProcModel;
		j["System"]["Processor"]["Id"]["Stepping"] = CPU::ProcStepping;
		j["System"]["Processor"]["Features"]["lm"] = ((CPU::GetAvailableFeatures() & CPU::feature::lm) != 0);
		j["System"]["Processor"]["Features"]["mmx"] = ((CPU::GetAvailableFeatures() & CPU::feature::mmx) != 0);
		j["System"]["Processor"]["Features"]["sse"] = ((CPU::GetAvailableFeatures() & CPU::feature::sse) != 0);
		j["System"]["Processor"]["Features"]["sse2"] = ((CPU::GetAvailableFeatures() & CPU::feature::sse2) != 0);
		j["System"]["Processor"]["Features"]["sse3"] = ((CPU::GetAvailableFeatures() & CPU::feature::sse3) != 0);
		j["System"]["Processor"]["Features"]["ssse3"] = ((CPU::GetAvailableFeatures() & CPU::feature::ssse3) != 0);
		j["System"]["Processor"]["Features"]["sse4.1"] = ((CPU::GetAvailableFeatures() & CPU::feature::sse4_1) != 0);
		j["System"]["Processor"]["Features"]["sse4.2"] = ((CPU::GetAvailableFeatures() & CPU::feature::sse4_2) != 0);
		j["System"]["Processor"]["Features"]["avx"] = ((CPU::GetAvailableFeatures() & CPU::feature::avx) != 0);
		j["System"]["Processor"]["Features"]["avx2"] = ((CPU::GetAvailableFeatures() & CPU::feature::avx2) != 0);
	#endif
	return j.dump(1, '\t');
}


#if MPT_UPDATE_LEGACY
mpt::ustring CUpdateCheck::GetUpdateURLV2(const CUpdateCheck::Settings &settings)
{
	mpt::ustring updateURL;
	if(settings.channel == UpdateChannelRelease)
	{
		updateURL = settings.channelReleaseURL;
		if(updateURL.empty())
		{
			updateURL = GetDefaultChannelReleaseURL();
		}
	}	else if(settings.channel == UpdateChannelNext)
	{
		updateURL = settings.channelNextURL;
		if(updateURL.empty())
		{
			updateURL = GetDefaultChannelNextURL();
		}
	}	else if(settings.channel == UpdateChannelDevelopment)
	{
		updateURL = settings.channelDevelopmentURL;
		if(updateURL.empty())
		{
			updateURL = GetDefaultChannelDevelopmentURL();
		}
	}	else
	{
		updateURL = settings.channelReleaseURL;
		if(updateURL.empty())
		{
			updateURL = GetDefaultChannelReleaseURL();
		}
	}
	if(updateURL.find(U_("://")) == mpt::ustring::npos)
	{
		updateURL = U_("https://") + updateURL;
	}
	// Build update URL
	updateURL = mpt::String::Replace(updateURL, U_("$VERSION"), MPT_UFORMAT("{}-{}-{}")
		( Version::Current()
		, BuildVariants().GuessCurrentBuildName()
		, settings.sendStatistics ? mpt::Windows::Version::Current().GetNameShort() : U_("unknown")
		));
	updateURL = mpt::String::Replace(updateURL, U_("$GUID"), settings.sendStatistics ? mpt::ufmt::val(settings.statisticsUUID) : U_("anonymous"));
	return updateURL;
}
#endif // MPT_UPDATE_LEGACY


// Run update check (independent thread)
CUpdateCheck::Result CUpdateCheck::SearchUpdate(const CUpdateCheck::Context &context, const CUpdateCheck::Settings &settings, const std::string &statistics)
{
	CUpdateCheck::Result result;
	if(!context.window->SendMessage(context.msgProgress, context.autoUpdate ? 1 : 0, 0))
	{
		throw CUpdateCheck::Cancel();
	}
	if(!context.window->SendMessage(context.msgProgress, context.autoUpdate ? 1 : 0, 20))
	{
		throw CUpdateCheck::Cancel();
	}
	HTTP::InternetSession internet(Version::Current().GetOpenMPTVersionString());
	if(!context.window->SendMessage(context.msgProgress, context.autoUpdate ? 1 : 0, 40))
	{
		throw CUpdateCheck::Cancel();
	}
#if MPT_UPDATE_LEGACY
	if(settings.modeLegacy)
	{
		result = SearchUpdateLegacy(internet, settings);
	} else
#endif // MPT_UPDATE_LEGACY
	{
		bool loaded = false;
		if(context.loadPersisted)
		{
			try
			{
				InputFile f(settings.persistencePath + P_("update-") + mpt::PathString::FromUnicode(GetChannelName(settings.channel)) + P_(".json"));
				if(f.IsValid())
				{
					std::vector<std::byte> data = GetFileReader(f).ReadRawDataAsByteVector();
					nlohmann::json::parse(mpt::buffer_cast<std::string>(data)).get<Update::versions>();
					result.CheckTime = time_t{};
					result.json = data;
					loaded = true;
				}
			} catch(mpt::out_of_memory e)
			{
				mpt::delete_out_of_memory(e);
			}	catch(const std::exception &)
			{
				// ignore
			}
		}
		if(!loaded)
		{
			result = SearchUpdateModern(internet, settings);
		}
		try
		{
			{
				mpt::SafeOutputFile f(settings.persistencePath + P_("update-") + mpt::PathString::FromUnicode(GetChannelName(settings.channel)) + P_(".json"), std::ios::binary);
				f.stream().imbue(std::locale::classic());
				mpt::IO::WriteRaw(f.stream(), mpt::as_span(result.json));
				f.stream().flush();
			}
		} catch(mpt::out_of_memory e)
		{
			mpt::delete_out_of_memory(e);
		} catch(const std::exception &)
		{
			// ignore
		}
	}
	if(!context.window->SendMessage(context.msgProgress, context.autoUpdate ? 1 : 0, 60))
	{
		throw CUpdateCheck::Cancel();
	}
	SendStatistics(internet, settings, statistics);
	if(!context.window->SendMessage(context.msgProgress, context.autoUpdate ? 1 : 0, 80))
	{
		throw CUpdateCheck::Cancel();
	}
	CleanOldUpdates(settings, context);
	if(!context.window->SendMessage(context.msgProgress, context.autoUpdate ? 1 : 0, 100))
	{
		throw CUpdateCheck::Cancel();
	}
	return result;
}


void CUpdateCheck::CleanOldUpdates(const CUpdateCheck::Settings & /* settings */ , const CUpdateCheck::Context & /* context */ )
{
	mpt::PathString dirTemp = mpt::GetTempDirectory();
	if(dirTemp.empty())
	{
		return;
	}
	if(PathIsRelative(dirTemp.AsNative().c_str()))
	{
		return;
	}
	if(!dirTemp.IsDirectory())
	{
		return;
	}
	mpt::PathString dirTempOpenMPT = dirTemp + P_("OpenMPT") + mpt::PathString::FromNative(mpt::RawPathString(1, mpt::PathString::GetDefaultPathSeparator()));
	mpt::PathString dirTempOpenMPTUpdates = dirTempOpenMPT + P_("Updates") + mpt::PathString::FromNative(mpt::RawPathString(1, mpt::PathString::GetDefaultPathSeparator()));
	mpt::DeleteWholeDirectoryTree(dirTempOpenMPTUpdates);
}


void CUpdateCheck::SendStatistics(HTTP::InternetSession &internet, const CUpdateCheck::Settings &settings, const std::string &statistics)
{
	if(settings.sendStatistics)
	{
		if(!settings.modeLegacy)
		{
			HTTP::Request requestLegacyUpdate;
			requestLegacyUpdate.SetURI(ParseURI(GetUpdateURLV2(settings)));
			requestLegacyUpdate.method = HTTP::Method::Get;
			requestLegacyUpdate.flags = HTTP::NoCache;
			HTTP::Result resultLegacyUpdateHTTP = internet(requestLegacyUpdate);
		}
		HTTP::Request requestStatistics;
		if(settings.statisticsUUID.IsValid())
		{
			requestStatistics.SetURI(ParseURI(settings.apiURL + MPT_UFORMAT("statistics/{}")(settings.statisticsUUID)));
			requestStatistics.method = HTTP::Method::Put;
		} else
		{
			requestStatistics.SetURI(ParseURI(settings.apiURL + U_("statistics/")));
			requestStatistics.method = HTTP::Method::Post;
		}
		requestStatistics.dataMimeType = HTTP::MimeType::JSON();
		requestStatistics.acceptMimeTypes = HTTP::MimeTypes::JSON();
		std::string jsondata = statistics;
		MPT_LOG(LogInformation, "Update", mpt::ToUnicode(mpt::Charset::UTF8, jsondata));
		requestStatistics.data = mpt::byte_cast<mpt::const_byte_span>(mpt::as_span(jsondata));
		internet(requestStatistics);
	}
}


#if MPT_UPDATE_LEGACY
CUpdateCheck::Result CUpdateCheck::SearchUpdateLegacy(HTTP::InternetSession &internet, const CUpdateCheck::Settings &settings)
{

	HTTP::Request request;
	request.SetURI(ParseURI(GetUpdateURLV2(settings)));
	request.method = HTTP::Method::Get;
	request.flags = HTTP::NoCache;

	HTTP::Result resultHTTP = internet(request);

	// Retrieve HTTP status code.
	if(resultHTTP.Status >= 400)
	{
		throw CUpdateCheck::Error(MPT_CFORMAT("Version information could not be found on the server (HTTP status code {}). Maybe your version of OpenMPT is too old!")(resultHTTP.Status));
	}

	// Now, evaluate the downloaded data.
	CUpdateCheck::Result result;
	result.UpdateAvailable = false;
	result.CheckTime = time(nullptr);
	CString resultData = mpt::ToCString(mpt::Charset::UTF8, mpt::buffer_cast<std::string>(resultHTTP.Data));
	if(resultData.CompareNoCase(_T("noupdate")) != 0)
	{
		CString token;
		int parseStep = 0, parsePos = 0;
		while(!(token = resultData.Tokenize(_T("\n"), parsePos)).IsEmpty())
		{
			token.Trim();
			switch(parseStep++)
			{
			case 0:
				if(token.CompareNoCase(_T("update")) != 0)
				{
					throw CUpdateCheck::Error(_T("Could not understand server response. Maybe your version of OpenMPT is too old!"));
				}
				break;
			case 1:
				result.Version = token;
				break;
			case 2:
				result.Date = token;
				break;
			case 3:
				result.URL = token;
				break;
			}
		}
		if(parseStep < 4)
		{
			throw CUpdateCheck::Error(_T("Could not understand server response. Maybe your version of OpenMPT is too old!"));
		}
		result.UpdateAvailable = true;
	}

	return result;

}
#endif // MPT_UPDATE_LEGACY


CUpdateCheck::Result CUpdateCheck::SearchUpdateModern(HTTP::InternetSession &internet, const CUpdateCheck::Settings &settings)
{

	HTTP::Request request;
	request.SetURI(ParseURI(settings.apiURL + MPT_UFORMAT("update/{}")(GetChannelName(static_cast<UpdateChannel>(settings.channel)))));
	request.method = HTTP::Method::Get;
	request.acceptMimeTypes = HTTP::MimeTypes::JSON();
	request.flags = HTTP::NoCache;

	HTTP::Result resultHTTP = internet(request);

	// Retrieve HTTP status code.
	if(resultHTTP.Status >= 400)
	{
		throw CUpdateCheck::Error(MPT_CFORMAT("Version information could not be found on the server (HTTP status code {}). Maybe your version of OpenMPT is too old!")(resultHTTP.Status));
	}

	// Now, evaluate the downloaded data.
	CUpdateCheck::Result result;
	result.CheckTime = time(nullptr);
	try
	{
		nlohmann::json::parse(mpt::buffer_cast<std::string>(resultHTTP.Data)).get<Update::versions>();
		result.json = resultHTTP.Data;
	} catch(mpt::out_of_memory e)
	{
		mpt::rethrow_out_of_memory(e);
	}	catch(const nlohmann::json::exception &e)
	{
		throw CUpdateCheck::Error(MPT_CFORMAT("Could not understand server response ({}). Maybe your version of OpenMPT is too old!")(mpt::get_exception_text<mpt::ustring>(e)));
	}

	return result;

}


void CUpdateCheck::CheckForUpdate(const CUpdateCheck::Settings &settings, const CUpdateCheck::Context &context)
{
	// incremented before starting the thread
	MPT_ASSERT(s_InstanceCount.load() >= 1);
	CUpdateCheck::Result result;
	try
	{
		context.window->SendMessage(context.msgStart, context.autoUpdate ? 1 : 0, 0);
		try
		{
			result = SearchUpdate(context, settings, context.statistics);
		} catch(const bad_uri &e)
		{
			throw CUpdateCheck::Error(MPT_CFORMAT("Error parsing update URL: {}")(mpt::get_exception_text<mpt::ustring>(e)));
		} catch(const HTTP::exception &e)
		{
			throw CUpdateCheck::Error(CString(_T("HTTP error: ")) + mpt::ToCString(e.GetMessage()));
		}
	} catch(const CUpdateCheck::Cancel &)
	{
		context.window->SendMessage(context.msgCanceled, context.autoUpdate ? 1 : 0, 0);
		s_InstanceCount.fetch_sub(1);
		MPT_ASSERT(s_InstanceCount.load() >= 0);
		return;
	} catch(const CUpdateCheck::Error &e)
	{
		context.window->SendMessage(context.msgFailure, context.autoUpdate ? 1 : 0, reinterpret_cast<LPARAM>(&e));
		s_InstanceCount.fetch_sub(1);
		MPT_ASSERT(s_InstanceCount.load() >= 0);
		return;
	}
	context.window->SendMessage(context.msgSuccess, context.autoUpdate ? 1 : 0, reinterpret_cast<LPARAM>(&result));
	s_InstanceCount.fetch_sub(1);
	MPT_ASSERT(s_InstanceCount.load() >= 0);
}


bool CUpdateCheck::IsAutoUpdateFromMessage(WPARAM wparam, LPARAM /* lparam */ )
{
	return wparam ? true : false;
}


CUpdateCheck::Result CUpdateCheck::ResultFromMessage(WPARAM /*wparam*/ , LPARAM lparam)
{
	const CUpdateCheck::Result &result = *reinterpret_cast<CUpdateCheck::Result*>(lparam);
	return result;
}


CUpdateCheck::Error CUpdateCheck::ErrorFromMessage(WPARAM /*wparam*/ , LPARAM lparam)
{
	const CUpdateCheck::Error &error = *reinterpret_cast<CUpdateCheck::Error*>(lparam);
	return error;
}



static const char * const updateScript = R"vbs(

Wscript.Echo
Wscript.Echo "OpenMPT portable Update"
Wscript.Echo "======================="

Wscript.Echo "[  0%] Waiting for OpenMPT to close..."
WScript.Sleep 2000

Wscript.Echo "[ 10%] Loading update settings..."
zip = WScript.Arguments.Item(0)
subfolder = WScript.Arguments.Item(1)
dst = WScript.Arguments.Item(2)
restartbinary = WScript.Arguments.Item(3)

Wscript.Echo "[ 20%] Preparing update..."
Set fso = CreateObject("Scripting.FileSystemObject")
Set shell = CreateObject("Wscript.Shell")
Set application = CreateObject("Shell.Application")

Sub CreateFolder(pathname)
	If Not fso.FolderExists(pathname) Then
		fso.CreateFolder pathname
	End If
End Sub

Sub DeleteFolder(pathname)
	If fso.FolderExists(pathname) Then
		fso.DeleteFolder pathname
	End If
End Sub

Sub UnZIP(zipfilename, destinationfolder)
	If Not fso.FolderExists(destinationfolder) Then
		fso.CreateFolder(destinationfolder)
	End If
	application.NameSpace(destinationfolder).Copyhere application.NameSpace(zipfilename).Items, 16+256
End Sub

Wscript.Echo "[ 30%] Changing to temporary directory..."
shell.CurrentDirectory = fso.GetParentFolderName(WScript.ScriptFullName)

Wscript.Echo "[ 40%] Decompressing update..."
UnZIP zip, fso.BuildPath(fso.GetAbsolutePathName("."), "tmp")

Wscript.Echo "[ 60%] Installing update..."
If subfolder = "" Or subfolder = "." Then
	fso.CopyFolder fso.BuildPath(fso.GetAbsolutePathName("."), "tmp"), dst, True
Else
	fso.CopyFolder fso.BuildPath(fso.BuildPath(fso.GetAbsolutePathName("."), "tmp"), subfolder), dst, True
End If

Wscript.Echo "[ 80%] Deleting temporary directory..."
DeleteFolder fso.BuildPath(fso.GetAbsolutePathName("."), "tmp")

Wscript.Echo "[ 90%] Restarting OpenMPT..."
application.ShellExecute fso.BuildPath(dst, restartbinary), , dst, , 10

Wscript.Echo "[100%] Update successful!"
Wscript.Echo
WScript.Sleep 1000

Wscript.Echo "Closing update window in 5 seconds..."
WScript.Sleep 1000
Wscript.Echo "Closing update window in 4 seconds..."
WScript.Sleep 1000
Wscript.Echo "Closing update window in 3 seconds..."
WScript.Sleep 1000
Wscript.Echo "Closing update window in 2 seconds..."
WScript.Sleep 1000
Wscript.Echo "Closing update window in 1 seconds..."
WScript.Sleep 1000
Wscript.Echo "Closing update window..."

WScript.Quit

)vbs";



class CDoUpdate: public CProgressDialog
{
private:
	Update::download download;
	class Aborted : public std::exception {};
	class Warning : public std::exception
	{
	private:
		mpt::ustring msg;
	public:
		Warning(const mpt::ustring &msg_)
			: msg(msg_)
		{
			return;
		}
		mpt::ustring get_msg() const
		{
			return msg;
		}
	};
	class Error : public std::exception
	{
	private:
		mpt::ustring msg;
	public:
		Error(const mpt::ustring &msg_)
			: msg(msg_)
		{
			return;
		}
		mpt::ustring get_msg() const
		{
			return msg;
		}
	};
public:
	CDoUpdate(Update::download download, CWnd *parent = NULL)
		: CProgressDialog(parent)
		, download(download)
	{
		return;
	}
	void UpdateProgress(const CString &text, double percent)
	{
		SetText(text);
		SetProgress(static_cast<uint64>(percent * 100.0));
		ProcessMessages();
		if(m_abort)
		{
			throw Aborted();
		}
	}
	void Run() override
	{
		try
		{
			SetTitle(_T("OpenMPT Update"));
			SetAbortText(_T("Cancel"));
			SetText(_T("OpenMPT Update"));
			SetRange(0, 10000);
			ProcessMessages();

			Update::downloadinfo downloadinfo;
			mpt::PathString dirTempOpenMPTUpdates;
			mpt::PathString updateFilename;
			{

				UpdateProgress(_T("Connecting..."), 0.0);
				HTTP::InternetSession internet(Version::Current().GetOpenMPTVersionString());

				UpdateProgress(_T("Downloading update information..."), 1.0);
				std::vector<std::byte> rawDownloadInfo;
				{
					HTTP::Request request;
					request.SetURI(ParseURI(download.url));
					request.method = HTTP::Method::Get;
					request.acceptMimeTypes = HTTP::MimeTypes::JSON();
					HTTP::Result resultHTTP = internet(request);
					if(resultHTTP.Status != 200)
					{
						throw Error(MPT_UFORMAT("Error downloading update information: HTTP status {}.")(resultHTTP.Status));
					}
					rawDownloadInfo = std::move(resultHTTP.Data);
				}

				if(!TrackerSettings::Instance().UpdateSkipSignatureVerificationUNSECURE)
				{
					std::vector<std::byte> rawSignature;
					UpdateProgress(_T("Retrieving update signature..."), 2.0);
					{
						HTTP::Request request;
						request.SetURI(ParseURI(download.url + U_(".jws.json")));
						request.method = HTTP::Method::Get;
						request.acceptMimeTypes = HTTP::MimeTypes::JSON();
						HTTP::Result resultHTTP = internet(request);
						if(resultHTTP.Status != 200)
						{
							throw Error(MPT_UFORMAT("Error downloading update signature: HTTP status {}.")(resultHTTP.Status));
						}
						rawSignature = std::move(resultHTTP.Data);
					}
					UpdateProgress(_T("Retrieving update signing public keys..."), 3.0);
					std::vector<mpt::crypto::asymmetric::rsassa_pss<>::public_key> keys;
					{
						std::vector<mpt::ustring> keyAnchors = TrackerSettings::Instance().UpdateSigningKeysRootAnchors;
						if(keyAnchors.empty())
						{
							Reporting::Warning(U_("Warning: No update signing public key root anchors configured. Update cannot be verified."), U_("OpenMPT Update"));
						}
						for(const auto & keyAnchor : keyAnchors)
						{
							HTTP::Request request;
							request.SetURI(ParseURI(keyAnchor + U_("signingkeys.jwkset.json")));
							request.method = HTTP::Method::Get;
							request.flags = HTTP::NoCache;
							request.acceptMimeTypes = HTTP::MimeTypes::JSON();
							try
							{
								HTTP::Result resultHTTP = internet(request);
								resultHTTP.CheckStatus(200);
								mpt::append(keys, mpt::crypto::asymmetric::rsassa_pss<>::parse_jwk_set(mpt::ToUnicode(mpt::Charset::UTF8, mpt::buffer_cast<std::string>(resultHTTP.Data))));
							} catch(mpt::out_of_memory e)
							{
								mpt::rethrow_out_of_memory(e);
							} catch(const std::exception &e)
							{
								Reporting::Warning(MPT_UFORMAT("Warning: Retrieving update signing public keys from {} failed: {}")(keyAnchor, mpt::get_exception_text<mpt::ustring>(e)), U_("OpenMPT Update"));
							} catch(...)
							{
								Reporting::Warning(MPT_UFORMAT("Warning: Retrieving update signing public keys from {} failed.")(keyAnchor), U_("OpenMPT Update"));
							}
						}
					}
					if(keys.empty())
					{
						throw Error(U_("Error retrieving update signing public keys."));
					}
					UpdateProgress(_T("Verifying signature..."), 4.0);
					std::vector<std::byte> expectedPayload = mpt::buffer_cast<std::vector<std::byte>>(rawDownloadInfo);
					mpt::ustring signature = mpt::ToUnicode(mpt::Charset::UTF8, mpt::buffer_cast<std::string>(rawSignature));

					mpt::crypto::asymmetric::rsassa_pss<>::jws_verify_at_least_one(keys, expectedPayload, signature);
			
				}

				UpdateProgress(_T("Parsing update information..."), 5.0);
				try
				{
					downloadinfo = nlohmann::json::parse(mpt::buffer_cast<std::string>(rawDownloadInfo)).get<Update::downloadinfo>();
				}	catch(const nlohmann::json::exception &e)
				{
					throw Error(MPT_UFORMAT("Error parsing update information: {}.")(mpt::get_exception_text<mpt::ustring>(e)));
				}

				UpdateProgress(_T("Preparing download..."), 6.0);
				mpt::PathString dirTemp = mpt::GetTempDirectory();
				mpt::PathString dirTempOpenMPT = dirTemp + P_("OpenMPT") + mpt::PathString::FromNative(mpt::RawPathString(1, mpt::PathString::GetDefaultPathSeparator()));
				dirTempOpenMPTUpdates = dirTempOpenMPT + P_("Updates") + mpt::PathString::FromNative(mpt::RawPathString(1, mpt::PathString::GetDefaultPathSeparator()));
				updateFilename = dirTempOpenMPTUpdates + mpt::PathString::FromUnicode(downloadinfo.filename);
				::CreateDirectory(dirTempOpenMPT.AsNativePrefixed().c_str(), NULL);
				::CreateDirectory(dirTempOpenMPTUpdates.AsNativePrefixed().c_str(), NULL);
			
				{
			
					UpdateProgress(_T("Creating file..."), 7.0);
					mpt::SafeOutputFile file(updateFilename, std::ios::binary);
					file.stream().imbue(std::locale::classic());
					file.stream().exceptions(std::ios::failbit | std::ios::badbit);
				
					UpdateProgress(_T("Downloading update..."), 8.0);
					HTTP::Request request;
					request.SetURI(ParseURI(downloadinfo.url));
					request.method = HTTP::Method::Get;
					request.acceptMimeTypes = HTTP::MimeTypes::Binary();
					request.outputStream = &file.stream();
					request.progressCallback = [&](HTTP::Progress progress, uint64 transferred, std::optional<uint64> expectedSize) {
						switch(progress)
						{
						case HTTP::Progress::Start:
							SetProgress(900);
							break;
						case HTTP::Progress::ConnectionEstablished:
							SetProgress(1000);
							break;
						case HTTP::Progress::RequestOpened:
							SetProgress(1100);
							break;
						case HTTP::Progress::RequestSent:
							SetProgress(1200);
							break;
						case HTTP::Progress::ResponseReceived:
							SetProgress(1300);
							break;
						case HTTP::Progress::TransferBegin:
							SetProgress(1400);
							break;
						case HTTP::Progress::TransferRunning:
							if(expectedSize && ((*expectedSize) != 0))
							{
								SetProgress(static_cast<int64>((static_cast<double>(transferred) / static_cast<double>(*expectedSize)) * (10000.0-1500.0-400.0) + 1500.0));
							} else
							{
								SetProgress((1500 + 9600) / 2);
							}
							break;
						case HTTP::Progress::TransferDone:
							SetProgress(9600);
							break;
						}
						ProcessMessages();
						if(m_abort)
						{
							throw HTTP::Abort();
						}
					};
					HTTP::Result resultHTTP = internet(request);
					if(resultHTTP.Status != 200)
					{
						throw Error(MPT_UFORMAT("Error downloading update: HTTP status {}.")(resultHTTP.Status));
					}
				}

				UpdateProgress(_T("Disconnecting..."), 97.0);
			}

			UpdateProgress(_T("Verifying download..."), 98.0);
			bool verified = false;
			for(const auto & [algorithm, value] : downloadinfo.checksums)
			{
				if(algorithm == U_("SHA-512"))
				{
					std::vector<std::byte> binhash = Util::HexToBin(value);
					if(binhash.size() != 512/8)
					{
						throw Error(U_("Download verification failed."));
					}
					std::array<std::byte, 512/8> expected;
					std::copy(binhash.begin(), binhash.end(), expected.begin());
					mpt::crypto::hash::SHA512 hash;
					mpt::ifstream f(updateFilename, std::ios::binary);
					f.imbue(std::locale::classic());
					f.exceptions(std::ios::badbit);
					while(!mpt::IO::IsEof(f))
					{
						std::array<std::byte, mpt::IO::BUFFERSIZE_TINY> buf;
						hash.process(mpt::IO::ReadRaw(f, mpt::as_span(buf)));
					}
					std::array<std::byte, 512/8> gotten = hash.result();
					if(gotten != expected)
					{
						throw Error(U_("Download verification failed."));
					}
					verified = true;
				}
			}
			if(!verified)
			{
				throw Error(U_("Error verifying update: No suitable checksum found."));
			}

			UpdateProgress(_T("Installing update..."), 99.0);
			bool wantClose = false;
			if(download.can_autoupdate && (Version::Current() >= Version::Parse(download.autoupdate_minversion)))
			{
				if(download.type == U_("installer") && downloadinfo.autoupdate_installer)
				{
					if(theApp.IsSourceTreeMode())
					{
						throw Warning(MPT_UFORMAT("Refusing to launch update '{} {}' when running from source tree.")(updateFilename, mpt::String::Combine(downloadinfo.autoupdate_installer->arguments, U_(" "))));
					}
					if(reinterpret_cast<INT_PTR>(ShellExecute(NULL, NULL,
						updateFilename.AsNative().c_str(),
						mpt::ToWin(mpt::String::Combine(downloadinfo.autoupdate_installer->arguments, U_(" "))).c_str(),
						dirTempOpenMPTUpdates.AsNative().c_str(),
						SW_SHOWDEFAULT)) < 32)
					{
						throw Error(U_("Error launching update."));
					}
				} else if(download.type == U_("archive") && downloadinfo.autoupdate_archive)
				{
					try
					{
						mpt::SafeOutputFile file(dirTempOpenMPTUpdates + P_("update.vbs"), std::ios::binary);
						file.stream().imbue(std::locale::classic());
						file.stream().exceptions(std::ios::failbit | std::ios::badbit);
						mpt::IO::WriteRaw(file.stream(), mpt::as_span(std::string(updateScript)));
					} catch(...)
					{
						throw Error(U_("Error creating update script."));
					}
					std::vector<mpt::ustring> arguments;
					arguments.push_back(U_("\"") + (dirTempOpenMPTUpdates + P_("update.vbs")).ToUnicode() + U_("\""));
					arguments.push_back(U_("\"") + updateFilename.ToUnicode() + U_("\""));
					arguments.push_back(U_("\"") + (downloadinfo.autoupdate_archive->subfolder.empty() ? U_(".") : downloadinfo.autoupdate_archive->subfolder) + U_("\""));
					arguments.push_back(U_("\"") + theApp.GetInstallPath().WithoutTrailingSlash().ToUnicode() + U_("\""));
					arguments.push_back(U_("\"") + downloadinfo.autoupdate_archive->restartbinary + U_("\""));
					if(theApp.IsSourceTreeMode())
					{
						throw Warning(MPT_UFORMAT("Refusing to launch update '{} {}' when running from source tree.")(P_("cscript.exe"), mpt::String::Combine(arguments, U_(" "))));
					}
					if(reinterpret_cast<INT_PTR>(ShellExecute(NULL, NULL,
						P_("cscript.exe").AsNative().c_str(),
						mpt::ToWin(mpt::String::Combine(arguments, U_(" "))).c_str(),
						dirTempOpenMPTUpdates.AsNative().c_str(),
						SW_SHOWDEFAULT)) < 32)
					{
						throw Error(U_("Error launching update."));
					}
					wantClose = true;
				} else
				{
					CTrackApp::OpenDirectory(dirTempOpenMPTUpdates);
					wantClose = true;
				}
			} else
			{
				CTrackApp::OpenDirectory(dirTempOpenMPTUpdates);
				wantClose = true;
			}
			UpdateProgress(_T("Waiting for installer..."), 100.0);
			if(wantClose)
			{
				CMainFrame::GetMainFrame()->PostMessage(WM_QUIT, 0, 0);
			}
			EndDialog(IDOK);
		} catch(mpt::out_of_memory e)
		{
			mpt::delete_out_of_memory(e);
			Reporting::Error(U_("Not enough memory to install update."), U_("OpenMPT Update Error"));
			EndDialog(IDCANCEL);
			return;
		} catch(const HTTP::Abort &)
		{
			EndDialog(IDCANCEL);
			return;
		} catch(const Aborted &)
		{
			EndDialog(IDCANCEL);
			return;
		} catch(const Warning &e)
		{
			Reporting::Warning(e.get_msg(), U_("OpenMPT Update"));
			EndDialog(IDCANCEL);
			return;
		} catch(const Error &e)
		{
			Reporting::Error(e.get_msg(), U_("OpenMPT Update Error"));
			EndDialog(IDCANCEL);
			return;
		} catch(const std::exception &e)
		{
			Reporting::Error(MPT_UFORMAT("Error installing update: {}")(mpt::get_exception_text<mpt::ustring>(e)), U_("OpenMPT Update Error"));
			EndDialog(IDCANCEL);
			return;
		} catch(...)
		{
			Reporting::Error(U_("Error installing update."), U_("OpenMPT Update Error"));
			EndDialog(IDCANCEL);
			return;
		}
	}
};


void CUpdateCheck::ShowSuccessGUI(WPARAM wparam, LPARAM lparam)
{

	const CUpdateCheck::Result &result = *reinterpret_cast<CUpdateCheck::Result*>(lparam);
	bool autoUpdate = wparam != 0;

	if(result.CheckTime != time_t{})
	{
		TrackerSettings::Instance().UpdateLastUpdateCheck = mpt::Date::Unix(result.CheckTime);
	}

#if MPT_UPDATE_LEGACY

	if(!TrackerSettings::Instance().UpdateExperimentalNewAutoUpdate)
	{
		if(result.UpdateAvailable && (!autoUpdate || result.Version != TrackerSettings::Instance().UpdateIgnoreVersion))
		{
			UpdateDialog dlg(result.Version, result.Date, result.URL);
			if(dlg.DoModal() == IDOK)
			{
				CTrackApp::OpenURL(result.URL);
			}
		} else if(!result.UpdateAvailable && !autoUpdate)
		{
			Reporting::Information(U_("You already have the latest version of OpenMPT installed."), U_("OpenMPT Internet Update"));
		}
		return;
	}

#endif // MPT_UPDATE_LEGACY

	Update::versions updateData = nlohmann::json::parse(mpt::buffer_cast<std::string>(result.json)).get<Update::versions>();
	UpdateInfo updateInfo = GetBestDownload(updateData);

	if(!updateInfo.IsAvailable())
	{
		if(!autoUpdate)
		{
			Reporting::Information(U_("You already have the latest version of OpenMPT installed."), U_("OpenMPT Update"));
		}
		return;
	}

	auto & versionInfo = updateData[updateInfo.version];
	if(autoUpdate && (mpt::ToCString(versionInfo.version) == TrackerSettings::Instance().UpdateIgnoreVersion))
	{
		return;
	}

	if(autoUpdate && TrackerSettings::Instance().UpdateInstallAutomatically && !updateInfo.download.empty() && versionInfo.downloads[updateInfo.download].can_autoupdate && (Version::Current() >= Version::Parse(versionInfo.downloads[updateInfo.download].autoupdate_minversion)))
	{

		CDoUpdate updateDlg(versionInfo.downloads[updateInfo.download], theApp.GetMainWnd());
		if(updateDlg.DoModal() != IDOK)
		{
			return;
		}

	} else
	{
		
		UpdateDialog dlg(
			mpt::ToCString(versionInfo.version),
			mpt::ToCString(versionInfo.date),
			mpt::ToCString(versionInfo.changelog_url),
				(!updateInfo.download.empty() && versionInfo.downloads[updateInfo.download].can_autoupdate && (Version::Current() >= Version::Parse(versionInfo.downloads[updateInfo.download].autoupdate_minversion))) ? _T("&Install now...") :
				(!updateInfo.download.empty()) ? _T("&Download now...") :
				_T("&View Announcement...")
			);
		if(dlg.DoModal() != IDOK)
		{
			return;
		}

		if(!updateInfo.download.empty() && versionInfo.downloads[updateInfo.download].can_autoupdate && (Version::Current() >= Version::Parse(versionInfo.downloads[updateInfo.download].autoupdate_minversion)))
		{
			CDoUpdate updateDlg(versionInfo.downloads[updateInfo.download], theApp.GetMainWnd());
			if(updateDlg.DoModal() != IDOK)
			{
				return;
			}
		} else if(!updateInfo.download.empty() && !versionInfo.downloads[updateInfo.download].download_url.empty())
		{
			CTrackApp::OpenURL(versionInfo.downloads[updateInfo.download].download_url);
		} else
		{
			CTrackApp::OpenURL(versionInfo.announcement_url);
		}

	}

}


mpt::ustring CUpdateCheck::GetFailureMessage(WPARAM wparam, LPARAM lparam)
{
	MPT_UNREFERENCED_PARAMETER(wparam);
	const CUpdateCheck::Error &error = *reinterpret_cast<CUpdateCheck::Error*>(lparam);
	return mpt::get_exception_text<mpt::ustring>(error);
}



void CUpdateCheck::ShowFailureGUI(WPARAM wparam, LPARAM lparam)
{
	const CUpdateCheck::Error &error = *reinterpret_cast<CUpdateCheck::Error*>(lparam);
	bool autoUpdate = wparam != 0;
	if(!autoUpdate)
	{
		Reporting::Error(mpt::get_exception_text<mpt::ustring>(error), U_("OpenMPT Update Error"));
	}
}


CUpdateCheck::Error::Error(CString errorMessage)
	: std::runtime_error(mpt::ToCharset(mpt::Charset::UTF8, errorMessage))
{
	return;
}


CUpdateCheck::Error::Error(CString errorMessage, DWORD errorCode)
	: std::runtime_error(mpt::ToCharset(mpt::Charset::UTF8, FormatErrorCode(errorMessage, errorCode)))
{
	return;
}


CString CUpdateCheck::Error::FormatErrorCode(CString errorMessage, DWORD errorCode)
{
	errorMessage += mpt::ToCString(Windows::GetErrorMessage(errorCode, GetModuleHandle(TEXT("wininet.dll"))));
	return errorMessage;
}



CUpdateCheck::Cancel::Cancel()
{
	return;
}


/////////////////////////////////////////////////////////////
// CUpdateSetupDlg

BEGIN_MESSAGE_MAP(CUpdateSetupDlg, CPropertyPage)
	ON_COMMAND(IDC_CHECK_UPDATEENABLED,         &CUpdateSetupDlg::OnSettingsChanged)
	ON_COMMAND(IDC_RADIO1,                      &CUpdateSetupDlg::OnSettingsChanged)
	ON_COMMAND(IDC_RADIO2,                      &CUpdateSetupDlg::OnSettingsChanged)
	ON_COMMAND(IDC_RADIO3,                      &CUpdateSetupDlg::OnSettingsChanged)
	ON_COMMAND(IDC_BUTTON1,                     &CUpdateSetupDlg::OnCheckNow)
	ON_CBN_SELCHANGE(IDC_COMBO_UPDATEFREQUENCY, &CUpdateSetupDlg::OnSettingsChanged)
	ON_COMMAND(IDC_CHECK_UPDATEINSTALLAUTOMATICALLY, &CUpdateSetupDlg::OnSettingsChanged)
	ON_COMMAND(IDC_CHECK1,                      &CUpdateSetupDlg::OnSettingsChanged)
	ON_NOTIFY(NM_CLICK, IDC_SYSLINK1,           &CUpdateSetupDlg::OnShowStatisticsData)
END_MESSAGE_MAP()


CUpdateSetupDlg::CUpdateSetupDlg()
	: CPropertyPage(IDD_OPTIONS_UPDATE)
	, m_SettingChangedNotifyGuard(theApp.GetSettings(), TrackerSettings::Instance().UpdateLastUpdateCheck.GetPath())
{
	return;
}


void CUpdateSetupDlg::DoDataExchange(CDataExchange *pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_UPDATEFREQUENCY, m_CbnUpdateFrequency);
}


BOOL CUpdateSetupDlg::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	CheckDlgButton(IDC_CHECK_UPDATEENABLED, TrackerSettings::Instance().UpdateEnabled ? BST_CHECKED : BST_UNCHECKED);

	int radioID = 0;
	uint32 updateChannel = TrackerSettings::Instance().UpdateChannel;
	if(updateChannel == UpdateChannelRelease)
	{
		radioID = IDC_RADIO1;
	} else if(updateChannel == UpdateChannelNext)
	{
		radioID = IDC_RADIO2;
	} else if(updateChannel == UpdateChannelDevelopment)
	{
		radioID = IDC_RADIO3;
	} else
	{
		radioID = IDC_RADIO1;
	}
	CheckRadioButton(IDC_RADIO1, IDC_RADIO3, radioID);

	int32 periodDays = TrackerSettings::Instance().UpdateIntervalDays;
	int ndx;

	ndx = m_CbnUpdateFrequency.AddString(_T("always"));
	m_CbnUpdateFrequency.SetItemData(ndx, 0);
	if(periodDays >= 0)
	{
		m_CbnUpdateFrequency.SetCurSel(ndx);
	}

	ndx = m_CbnUpdateFrequency.AddString(_T("daily"));
	m_CbnUpdateFrequency.SetItemData(ndx, 1);
	if(periodDays >= 1)
	{
		m_CbnUpdateFrequency.SetCurSel(ndx);
	}

	ndx = m_CbnUpdateFrequency.AddString(_T("weekly"));
	m_CbnUpdateFrequency.SetItemData(ndx, 7);
	if(periodDays >= 7)
	{
		m_CbnUpdateFrequency.SetCurSel(ndx);
	}

	ndx = m_CbnUpdateFrequency.AddString(_T("monthly"));
	m_CbnUpdateFrequency.SetItemData(ndx, 30);
	if(periodDays >= 30)
	{
		m_CbnUpdateFrequency.SetCurSel(ndx);
	}

	ndx = m_CbnUpdateFrequency.AddString(_T("never"));
	m_CbnUpdateFrequency.SetItemData(ndx, ~(DWORD_PTR)0);
	if(periodDays < 0)
	{		
		m_CbnUpdateFrequency.SetCurSel(ndx);
	}

	CheckDlgButton(IDC_CHECK_UPDATEINSTALLAUTOMATICALLY, TrackerSettings::Instance().UpdateInstallAutomatically ? BST_CHECKED : BST_UNCHECKED);

	CheckDlgButton(IDC_CHECK1, TrackerSettings::Instance().UpdateStatistics ? BST_CHECKED : BST_UNCHECKED);

	GetDlgItem(IDC_STATIC_UPDATEPRIVACYTEXT)->SetWindowText(mpt::ToCString(CUpdateCheck::GetStatisticsUserInformation(true)));

	EnableDisableDialog();

	m_SettingChangedNotifyGuard.Register(this);
	SettingChanged(TrackerSettings::Instance().UpdateLastUpdateCheck.GetPath());

	return TRUE;
}


void CUpdateSetupDlg::OnShowStatisticsData(NMHDR * /*pNMHDR*/, LRESULT * /*pResult*/)
{
	CUpdateCheck::Settings settings;

	uint32 updateChannel = TrackerSettings::Instance().UpdateChannel;
	if(IsDlgButtonChecked(IDC_RADIO1)) updateChannel = UpdateChannelRelease;
	if(IsDlgButtonChecked(IDC_RADIO2)) updateChannel = UpdateChannelNext;
	if(IsDlgButtonChecked(IDC_RADIO3)) updateChannel = UpdateChannelDevelopment;

	int updateCheckPeriod = (m_CbnUpdateFrequency.GetItemData(m_CbnUpdateFrequency.GetCurSel()) == ~(DWORD_PTR)0) ? -1 : static_cast<int>(m_CbnUpdateFrequency.GetItemData(m_CbnUpdateFrequency.GetCurSel()));

	settings.periodDays = updateCheckPeriod;
	settings.channel = static_cast<UpdateChannel>(updateChannel);
	settings.sendStatistics = (IsDlgButtonChecked(IDC_CHECK1) != BST_UNCHECKED);

	mpt::ustring statistics;

	statistics += U_("Update:") + UL_("\n");
	statistics += UL_("\n");

#if MPT_UPDATE_LEGACY
	if(settings.modeLegacy)
	{
		statistics += U_("GET ") + CUpdateCheck::GetUpdateURLV2(settings) + UL_("\n");
		statistics += UL_("\n");
	} else
#endif // MPT_UPDATE_LEGACY
	{
		statistics += U_("GET ") + settings.apiURL + MPT_UFORMAT("update/{}")(GetChannelName(static_cast<UpdateChannel>(settings.channel))) + UL_("\n");
		statistics += UL_("\n");
		std::vector<mpt::ustring> keyAnchors = TrackerSettings::Instance().UpdateSigningKeysRootAnchors;
		for(const auto & keyAnchor : keyAnchors)
		{
			statistics += U_("GET ") + keyAnchor + U_("signingkeys.jwkset.json") + UL_("\n");
			statistics += UL_("\n");
		}
	}

	if(settings.sendStatistics)
	{
		statistics += U_("Statistics:") + UL_("\n");
		statistics += UL_("\n");
#if MPT_UPDATE_LEGACY
		if(!settings.modeLegacy)
#endif // MPT_UPDATE_LEGACY
		{
			statistics += U_("GET ") + CUpdateCheck::GetUpdateURLV2(settings) + UL_("\n");
			statistics += UL_("\n");
		}
		if(settings.statisticsUUID.IsValid())
		{
			statistics += U_("PUT ") + settings.apiURL + MPT_UFORMAT("statistics/{}")(settings.statisticsUUID) + UL_("\n");
		} else
		{
			statistics += U_("POST ") + settings.apiURL + U_("statistics/") + UL_("\n");
		}
		statistics += mpt::String::Replace(mpt::ToUnicode(mpt::Charset::UTF8, CUpdateCheck::GetStatisticsDataV3(settings)), U_("\t"), U_("    "));
	}

	InfoDialog dlg(this);
	dlg.SetCaption(_T("Update Statistics Data"));
	dlg.SetContent(mpt::ToWin(mpt::String::Replace(statistics, U_("\n"), U_("\r\n"))));
	dlg.DoModal();
}


void CUpdateSetupDlg::SettingChanged(const SettingPath &changedPath)
{
	if(changedPath == TrackerSettings::Instance().UpdateLastUpdateCheck.GetPath())
	{
		CString updateText;
		const time_t t = TrackerSettings::Instance().UpdateLastUpdateCheck.Get();
		if(t > 0)
		{
			const tm* const lastUpdate = localtime(&t);
			if(lastUpdate != nullptr)
			{
				updateText.Format(_T("The last successful update check was run on %04d-%02d-%02d, %02d:%02d."), lastUpdate->tm_year + 1900, lastUpdate->tm_mon + 1, lastUpdate->tm_mday, lastUpdate->tm_hour, lastUpdate->tm_min);
			}
		}
		updateText += _T("\r\n");
		SetDlgItemText(IDC_LASTUPDATE, updateText);
	}
}


void CUpdateSetupDlg::EnableDisableDialog()
{

	BOOL status = ((IsDlgButtonChecked(IDC_CHECK_UPDATEENABLED) != BST_UNCHECKED) ? TRUE : FALSE);

	GetDlgItem(IDC_STATIC_UDATECHANNEL)->EnableWindow(status);
	GetDlgItem(IDC_RADIO1)->EnableWindow(status);
	GetDlgItem(IDC_RADIO2)->EnableWindow(status);
	GetDlgItem(IDC_RADIO3)->EnableWindow(status);

	GetDlgItem(IDC_STATIC_UPDATECHECK)->EnableWindow(status);
	GetDlgItem(IDC_STATIC_UPDATEFREQUENCY)->EnableWindow(status);
	GetDlgItem(IDC_COMBO_UPDATEFREQUENCY)->EnableWindow(status);
	GetDlgItem(IDC_BUTTON1)->EnableWindow(status);
	GetDlgItem(IDC_LASTUPDATE)->EnableWindow(status);
	GetDlgItem(IDC_CHECK_UPDATEINSTALLAUTOMATICALLY)->EnableWindow(status);

	GetDlgItem(IDC_STATIC_UPDATEPRIVACY)->EnableWindow(status);
	GetDlgItem(IDC_CHECK1)->EnableWindow(status);
	GetDlgItem(IDC_STATIC_UPDATEPRIVACYTEXT)->EnableWindow(status);
	GetDlgItem(IDC_SYSLINK1)->EnableWindow(status);
}


void CUpdateSetupDlg::OnSettingsChanged()
{
	EnableDisableDialog();
	SetModified(TRUE);
}


void CUpdateSetupDlg::OnOK()
{
	uint32 updateChannel = TrackerSettings::Instance().UpdateChannel;
	if(IsDlgButtonChecked(IDC_RADIO1)) updateChannel = UpdateChannelRelease;
	if(IsDlgButtonChecked(IDC_RADIO2)) updateChannel = UpdateChannelNext;
	if(IsDlgButtonChecked(IDC_RADIO3)) updateChannel = UpdateChannelDevelopment;

	int updateCheckPeriod = (m_CbnUpdateFrequency.GetItemData(m_CbnUpdateFrequency.GetCurSel()) == ~(DWORD_PTR)0) ? -1 : static_cast<int>(m_CbnUpdateFrequency.GetItemData(m_CbnUpdateFrequency.GetCurSel()));
	
	TrackerSettings::Instance().UpdateEnabled = (IsDlgButtonChecked(IDC_CHECK_UPDATEENABLED) != BST_UNCHECKED);

	TrackerSettings::Instance().UpdateIntervalDays = updateCheckPeriod;
	TrackerSettings::Instance().UpdateInstallAutomatically = (IsDlgButtonChecked(IDC_CHECK_UPDATEINSTALLAUTOMATICALLY) != BST_UNCHECKED);
	TrackerSettings::Instance().UpdateChannel = updateChannel;
	TrackerSettings::Instance().UpdateStatistics = (IsDlgButtonChecked(IDC_CHECK1) != BST_UNCHECKED);
	
	CPropertyPage::OnOK();
}


BOOL CUpdateSetupDlg::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_UPDATE;
	return CPropertyPage::OnSetActive();
}


void CUpdateSetupDlg::OnCheckNow()
{
	CUpdateCheck::DoManualUpdateCheck();
}


OPENMPT_NAMESPACE_END

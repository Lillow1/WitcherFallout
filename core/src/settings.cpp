#include "witcher/settings.h"

#include <cctype>
#include <charconv>
#include <cstdlib>
#include <functional>
#include <string>
#include <unordered_map>

namespace witcher
{
	namespace
	{
		std::string_view Trim(std::string_view a_s)
		{
			while (!a_s.empty() && std::isspace(static_cast<unsigned char>(a_s.front()))) {
				a_s.remove_prefix(1);
			}
			while (!a_s.empty() && std::isspace(static_cast<unsigned char>(a_s.back()))) {
				a_s.remove_suffix(1);
			}
			return a_s;
		}

		std::string Lower(std::string_view a_s)
		{
			std::string out(a_s);
			for (auto& c : out) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return out;
		}

		bool ParseFloat(std::string_view a_s, float& a_out)
		{
			// std::from_chars for float is not available on every toolchain we target.
			const std::string tmp(a_s);
			char* end = nullptr;
			const float v = std::strtof(tmp.c_str(), &end);
			if (end == tmp.c_str() || *end != '\0') {
				return false;
			}
			a_out = v;
			return true;
		}

		bool ParseUInt(std::string_view a_s, std::uint32_t& a_out)
		{
			int base = 10;
			if (a_s.size() > 2 && a_s[0] == '0' && (a_s[1] == 'x' || a_s[1] == 'X')) {
				a_s.remove_prefix(2);
				base = 16;
			}
			const auto res = std::from_chars(a_s.data(), a_s.data() + a_s.size(), a_out, base);
			return res.ec == std::errc{} && res.ptr == a_s.data() + a_s.size();
		}

		bool ParseBool(std::string_view a_s, bool& a_out)
		{
			const auto s = Lower(a_s);
			if (s == "1" || s == "true" || s == "on" || s == "yes") {
				a_out = true;
				return true;
			}
			if (s == "0" || s == "false" || s == "off" || s == "no") {
				a_out = false;
				return true;
			}
			return false;
		}

		using Setter = std::function<bool(std::string_view)>;

		Setter F(float& a_ref)
		{
			return [&a_ref](std::string_view v) { return ParseFloat(v, a_ref); };
		}
		Setter U(std::uint32_t& a_ref)
		{
			return [&a_ref](std::string_view v) { return ParseUInt(v, a_ref); };
		}
		Setter B(bool& a_ref)
		{
			return [&a_ref](std::string_view v) { return ParseBool(v, a_ref); };
		}

		void AddFraming(std::unordered_map<std::string, Setter>& a_map, const std::string& a_section, FramingPreset& a_p)
		{
			a_map[a_section + ".fshoulderx"] = F(a_p.shoulderX);
			a_map[a_section + ".fshoulderz"] = F(a_p.shoulderZ);
			a_map[a_section + ".fdistance"] = F(a_p.distance);
			a_map[a_section + ".ffov"] = F(a_p.fov);
		}

		void AddTargeting(std::unordered_map<std::string, Setter>& a_map, const std::string& a_section, TargetingSettings& a_t)
		{
			a_map[a_section + ".fmaxdistance"] = F(a_t.maxDistance);
			a_map[a_section + ".fmaxangledeg"] = F(a_t.maxAngleDeg);
			a_map[a_section + ".fangleweight"] = F(a_t.angleWeight);
			a_map[a_section + ".fdistanceweight"] = F(a_t.distanceWeight);
			a_map[a_section + ".bhostileonly"] = B(a_t.hostileOnly);
		}
	}

	Settings ParseSettings(std::string_view a_ini, std::vector<std::string>* a_warnings)
	{
		Settings s{};
		auto& cam = s.camera;
		auto& lock = s.lockOn;
		auto& soft = s.softLock;
		auto& in = s.input;

		std::unordered_map<std::string, Setter> keys;
		AddFraming(keys, "framing.exploration", cam.exploration);
		AddFraming(keys, "framing.combat", cam.combat);
		AddFraming(keys, "framing.melee", cam.melee);

		keys["camera.fframingblendtime"] = F(cam.framingBlendTime);
		keys["camera.flockyawsmoothtime"] = F(cam.lockYawSmoothTime);
		keys["camera.flockpitchsmoothtime"] = F(cam.lockPitchSmoothTime);
		keys["camera.flockpitchbiasdeg"] = F(cam.lockPitchBiasDeg);
		keys["camera.flockpitchmindeg"] = F(cam.lockPitchMinDeg);
		keys["camera.flockpitchmaxdeg"] = F(cam.lockPitchMaxDeg);
		keys["camera.flockmindistance"] = F(cam.lockMinDistance);
		keys["camera.fgroupradius"] = F(cam.groupRadius);
		keys["camera.fgroupzoomperenemy"] = F(cam.groupZoomPerEnemy);
		keys["camera.fgroupzoommax"] = F(cam.groupZoomMax);
		keys["camera.flargetargetzoom"] = F(cam.largeTargetZoom);
		keys["camera.fshakemaxyawdeg"] = F(cam.shakeMaxYawDeg);
		keys["camera.fshakemaxpitchdeg"] = F(cam.shakeMaxPitchDeg);
		keys["camera.fshakedecay"] = F(cam.shakeDecay);
		keys["camera.fshakefrequency"] = F(cam.shakeFrequency);

		AddTargeting(keys, "lockon", lock.targeting);
		keys["lockon.fbreakdistance"] = F(lock.breakDistance);
		keys["lockon.flosgraceseconds"] = F(lock.losGraceSeconds);
		keys["lockon.fswitchthreshold"] = F(lock.switchThreshold);
		keys["lockon.fswitchcooldown"] = F(lock.switchCooldown);
		keys["lockon.fswitchdecay"] = F(lock.switchDecay);
		keys["lockon.breleaseontargetdeath"] = B(lock.releaseOnTargetDeath);

		keys["softlock.benabled"] = B(soft.enabled);
		AddTargeting(keys, "softlock", soft.targeting);
		keys["softlock.fsnaptime"] = F(soft.snapTime);

		auto& dir = s.directional;
		keys["directional.benabled"] = B(dir.enabled);
		keys["directional.fturnratedeg"] = F(dir.turnRateDeg);
		keys["directional.faimturnratedeg"] = F(dir.aimTurnRateDeg);
		keys["directional.fattacksnaptime"] = F(dir.attackSnapTime);
		keys["directional.fattackholdtime"] = F(dir.attackHoldTime);
		keys["directional.fmovedeadzone"] = F(dir.moveDeadzone);
		keys["directional.brangedfacescamera"] = B(dir.rangedFacesCamera);
		keys["directional.flockrecentertime"] = F(dir.lockRecenterTime);

		keys["input.ilockonkey"] = U(in.lockOnKey);
		keys["input.ishoulderswapkey"] = U(in.shoulderSwapKey);
		keys["input.idirectionaltogglekey"] = U(in.directionalToggleKey);
		keys["input.ilockongamepad"] = U(in.lockOnGamepad);
		keys["input.fmouseflickscale"] = F(in.mouseFlickScale);
		keys["input.fstickflickscale"] = F(in.stickFlickScale);

		auto warn = [&](std::size_t a_line, const std::string& a_msg) {
			if (a_warnings) {
				a_warnings->push_back("line " + std::to_string(a_line) + ": " + a_msg);
			}
		};

		std::string section;
		std::size_t lineNo = 0;
		while (!a_ini.empty()) {
			++lineNo;
			const auto nl = a_ini.find('\n');
			std::string_view line = a_ini.substr(0, nl);
			a_ini.remove_prefix(nl == std::string_view::npos ? a_ini.size() : nl + 1);

			if (const auto c = line.find_first_of(";#"); c != std::string_view::npos) {
				line = line.substr(0, c);
			}
			line = Trim(line);
			if (line.empty()) {
				continue;
			}

			if (line.front() == '[') {
				if (line.back() != ']') {
					warn(lineNo, "malformed section header");
					continue;
				}
				section = Lower(Trim(line.substr(1, line.size() - 2)));
				continue;
			}

			const auto eq = line.find('=');
			if (eq == std::string_view::npos) {
				warn(lineNo, "expected key = value");
				continue;
			}
			const auto key = section + "." + Lower(Trim(line.substr(0, eq)));
			const auto value = Trim(line.substr(eq + 1));

			const auto it = keys.find(key);
			if (it == keys.end()) {
				warn(lineNo, "unknown setting '" + key + "'");
				continue;
			}
			if (!it->second(value)) {
				warn(lineNo, "bad value for '" + key + "'");
			}
		}

		return s;
	}
}

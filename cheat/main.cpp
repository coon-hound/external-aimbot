#include "memory.h"
#include "vector.h"

#include <thread>
#include <iostream>

namespace offset
{
	// client
	constexpr ::std::ptrdiff_t dwLocalPlayer = 0xDEA964;
	constexpr ::std::ptrdiff_t dwEntityList = 0x4DFFFB4;

	// engine
	constexpr ::std::ptrdiff_t dwClientState = 0x59F19C;
	constexpr ::std::ptrdiff_t dwClientState_GetLocalPlayer = 0x180;
	constexpr ::std::ptrdiff_t dwClientState_ViewAngles = 0x4D90;
	constexpr ::std::ptrdiff_t dwForceAttack = 0x322DDEC;

	// entity
	constexpr ::std::ptrdiff_t m_dwBoneMatrix = 0x26A8;
	constexpr ::std::ptrdiff_t m_bDormant = 0xED;
	constexpr ::std::ptrdiff_t m_iTeamNum = 0xF4;
	constexpr ::std::ptrdiff_t m_lifeState = 0x25F;
	constexpr ::std::ptrdiff_t m_vecOrigin = 0x138;
	constexpr ::std::ptrdiff_t m_vecViewOffset = 0x108;
	constexpr ::std::ptrdiff_t m_aimPunchAngle = 0x303C;
	constexpr ::std::ptrdiff_t m_bSpottedByMask = 0x980;
	constexpr ::std::ptrdiff_t m_iShotsFired = 0x103E0;
}

constexpr Vector3 CalculateAngle(
	const Vector3& localPosition,
	const Vector3& enemyPosition,
	const Vector3& viewAngles) noexcept
{
	return ((enemyPosition - localPosition).ToAngle() - viewAngles);
}

struct Vector2
{
	float x = { }, y = { };
};

int main()
{
	// initialize memory class
	const auto memory = Memory{ "csgo.exe" };

	// module addresses
	const auto client = memory.GetModuleAddress("client.dll");
	const auto engine = memory.GetModuleAddress("engine.dll");

	// infinite hack loop
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		// aimbot key
		if (!GetAsyncKeyState(0x47)) {
			continue;
		}
		std::printf("button pressed\n");



		// get local player
		const auto localPlayer = memory.Read<std::uintptr_t>(client + offset::dwLocalPlayer);
		const auto localTeam = memory.Read<std::int32_t>(localPlayer + offset::m_iTeamNum);

		// eye position = origin + viewOffset
		const auto localEyePosition = memory.Read<Vector3>(localPlayer + offset::m_vecOrigin) +
			memory.Read<Vector3>(localPlayer + offset::m_vecViewOffset);

		const auto clientState = memory.Read<std::uintptr_t>(engine + offset::dwClientState);

		const auto localPlayerId =
			memory.Read<std::int32_t>(clientState + offset::dwClientState_GetLocalPlayer);

		const auto viewAngles = memory.Read<Vector3>(clientState + offset::dwClientState_ViewAngles);
		const auto aimPunch = memory.Read<Vector3>(localPlayer + offset::m_aimPunchAngle) * 2;

		const auto& shotsFired = memory.Read<std::int32_t>(localPlayer + offset::m_iShotsFired);

		auto oldPunch = Vector2{ };

		// aimbot fov
		auto bestFov = 360.f;
		//auto bestFov = 5.f;
		auto bestAngle = Vector3{ };

		for (auto i = 1; i <= 32; ++i)
		{
			const auto player = memory.Read<std::uintptr_t>(client + offset::dwEntityList + i * 0x10);

			if (memory.Read<std::int32_t>(player + offset::m_iTeamNum) == localTeam)
//				continue;

			if (memory.Read<bool>(player + offset::m_bDormant))
				continue;

			if (memory.Read<std::int32_t>(player + offset::m_lifeState))
				continue;

			if (memory.Read<std::int32_t>(player + offset::m_bSpottedByMask) & (1 << localPlayerId))
			{
				const auto boneMatrix = memory.Read<std::uintptr_t>(player + offset::m_dwBoneMatrix);

				// pos of player head in 3d space
				// 8 is the head bone index :)
				const auto playerHeadPosition = Vector3{
					memory.Read<float>(boneMatrix + 0x30 * 8 + 0x0C),
					memory.Read<float>(boneMatrix + 0x30 * 8 + 0x1C),
					memory.Read<float>(boneMatrix + 0x30 * 8 + 0x2C)
				};

				const auto angle = CalculateAngle(
					localEyePosition,
					playerHeadPosition,
					viewAngles + aimPunch
				);

				const auto fov = std::hypot(angle.x, angle.y);

				if (fov < bestFov)
				{
					bestFov = fov;
					bestAngle = angle;
				}
			}
		}

		if (shotsFired) {
			const auto& aimPunch2 = memory.Read<Vector2>(localPlayer + offset::m_aimPunchAngle);
			const auto& viewAngles2 = memory.Read<Vector2>(localPlayer + offset::dwClientState_ViewAngles);
			
			auto newAngles = Vector2
			{
				viewAngles2.x + oldPunch.x - aimPunch2.x * 2.f,
				viewAngles2.y + oldPunch.y - aimPunch2.y * 2.f
			};

			if (newAngles.x > 89.f) {
				newAngles.x = 89.f;
			}

			if (newAngles.x < -89.f) {
				newAngles.x = -89.f;
			}

			if (newAngles.y > 180.f) {
				newAngles.y -= 360.f;
			}

			if (newAngles.y < 180.f) {
				newAngles.y += 360.f;
			}

			memory.Write<Vector2>(clientState + offset::dwClientState_ViewAngles, newAngles);
		}
		else {
			oldPunch.x = 0;
			oldPunch.y = 0;
		}

		// if we have a best angle, do aimbot
		if (!bestAngle.IsZero()) {
			//	memory.Write<Vector3>(clientState + offset::dwClientState_ViewAngles, viewAngles + bestAngle / 3.f); // smoothing
			memory.Write<Vector3>(clientState + offset::dwClientState_ViewAngles, viewAngles + bestAngle);
			memory.Write<int32_t>(client + offset::dwForceAttack, 1);
		}
		else {
			memory.Write<int32_t>(client + offset::dwForceAttack, 0);
		}
	}

	return 0;
}
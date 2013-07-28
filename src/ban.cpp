//////////////////////////////////////////////////////////////////////
// The Forgotten Server - a server application for the MMORPG Tibia
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////

#include "otpch.h"

#include "definitions.h"

#include "ban.h"
#include "iologindata.h"
#include "configmanager.h"
#include "tools.h"
#include "database.h"

extern ConfigManager g_config;

bool Ban::acceptConnection(uint32_t clientip)
{
	boost::recursive_mutex::scoped_lock lockClass(banLock);

	uint64_t currentTime = OTSYS_TIME();

	auto it = ipConnectMap.find(clientip);
	if (it == ipConnectMap.end()) {
		ConnectBlock cb;
		cb.lastAttempt = currentTime;
		cb.blockTime = 0;
		cb.count = 1;
		ipConnectMap[clientip] = cb;
		return true;
	}

	ConnectBlock& connectBlock = it->second;
	if (connectBlock.blockTime > currentTime) {
		connectBlock.blockTime += 225;
		return false;
	}

	int64_t timeDiff = currentTime - connectBlock.lastAttempt;
	connectBlock.lastAttempt = currentTime;
	if (timeDiff <= 8000) {
		if (++connectBlock.count > 3) {
			connectBlock.count = 0;
			if (timeDiff <= 800) {
				connectBlock.blockTime = currentTime + 3000;
				return false;
			}
		}
	} else {
		connectBlock.count = 1;
	}
	return true;
}

bool IOBan::isAccountBanned(uint32_t accountId, BanInfo& banInfo)
{
	Database* db = Database::getInstance();

	std::ostringstream query;
	query << "SELECT `reason`, `expires_at`, (SELECT `name` FROM `players` WHERE `id` = `banned_by`) AS `name` FROM `account_bans` WHERE `account_id` = " << accountId;

	DBResult* result = db->storeQuery(query.str());
	if (!result) {
		return false;
	}

	int64_t expiresAt = result->getDataLong("expires_at");
	if (expiresAt != 0 && time(NULL) > expiresAt) {
		db->freeResult(result);
		return false;
	}

	banInfo.expiresAt = expiresAt;
	banInfo.reason = result->getDataString("reason");
	banInfo.bannedBy = result->getDataString("name");
	db->freeResult(result);
	return true;
}

bool IOBan::isIpBanned(uint32_t clientip, BanInfo& banInfo)
{
	if (clientip == 0) {
		return false;
	}

	Database* db = Database::getInstance();

	std::ostringstream query;
	query << "SELECT `reason`, `expires_at`, (SELECT `name` FROM `players` WHERE `id` = `banned_by`) AS `name` FROM `ip_bans` WHERE `ip` = " << clientip;

	DBResult* result = db->storeQuery(query.str());
	if (!result) {
		return false;
	}

	int64_t expiresAt = result->getDataLong("expires_at");
	if (expiresAt != 0 && time(NULL) > expiresAt) {
		db->freeResult(result);
		return false;
	}

	banInfo.expiresAt = expiresAt;
	banInfo.reason = result->getDataString("reason");
	banInfo.bannedBy = result->getDataString("name");
	db->freeResult(result);
	return true;
}

bool IOBan::isPlayerNamelocked(uint32_t playerId)
{
	Database* db = Database::getInstance();

	std::ostringstream query;
	query << "SELECT 1 FROM `player_namelocks` WHERE `player_id` = " << playerId;

	DBResult* result = db->storeQuery(query.str());
	if (!result) {
		return false;
	}

	db->freeResult(result);
	return true;
}

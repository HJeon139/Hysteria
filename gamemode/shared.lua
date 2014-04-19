--
-- shared.lua
--


-- Include the required lua files.
include("config.lua")
include("player.lua")

-- Include the configuration for this map.
if file.Exists("../gamemodes/prop_hunt/gamemode/maps/"..game.GetMap()..".lua", "LUA") || file.Exists("../lua_temp/prop_hunt/gamemode/maps/"..game.GetMap()..".lua", "LUA") then

	include("maps/"..game.GetMap()..".lua")
	
end


-- Player classes
include("player_class/player_hunter.lua")
include("player_class/player_prop.lua")


-- Information about the gamemode.
GM.Name		= "Hysteria"
GM.Author	= "Poject_Hysteria"
GM.Email	= "hjeon333@ufl.edu"
GM.Website	= ""



DeriveGamemode("fretta")
-- Gamemode configuration.
GM.AutomaticTeamBalance		= true
GM.AddFragsToTeamScore		= true
GM.Data 					= {}
GM.EnableFreezeCam			= false
GM.GameLength				= GAME_TIME
GM.NoAutomaticSpawning		= true
GM.NoNonPlayerPlayerDamage	= false
GM.NoPlayerPlayerDamage 	= false
GM.RoundBased				= true
GM.RoundLimit				= ROUNDS_PER_MAP
GM.RoundLength 				= ROUND_TIME
GM.RoundPreStartTime		= 0
GM.SelectModel				= false
GM.ShowTeamName 		= false
GM.SuicideString			= "couldn't take the pressure and committed suicide."
GM.TeamBased 				= false
GM.HudSkin 					= "PropHuntSkin" -- The Derma skin to use for the HUD components
GM.RoundPostLength 			= 8	-- Seconds to show the 'x team won!' screen at the end of a round

GM.ValidSpectatorModes = { OBS_MODE_CHASE, OBS_MODE_IN_EYE, OBS_MODE_ROAMING }
GM.ValidSpectatorEntities = { "player" }	// Entities we can spectate
GM.CanOnlySpectateOwnTeam = false

TEAM_HUNTERS = 1
TEAM_PROPS = 2
-- Called on gamemdoe initialization to create teams.
function GM:CreateTeams()
	
	
	team.SetUp(TEAM_HUNTERS, "Humans", Color(150, 205, 255, 255))
	team.SetSpawnPoint(TEAM_HUNTERS, {"info_player_counterterrorist", "info_player_combine", "info_player_deathmatch", "info_player_axis"})
	team.SetClass(TEAM_HUNTERS, {"Humans"})

	
	team.SetUp(TEAM_PROPS, "Infected", Color(255, 60, 60, 255))
	team.SetSpawnPoint(TEAM_PROPS, {"info_player_terrorist", "info_player_rebel", "info_player_deathmatch", "info_player_allies"})
	team.SetClass(TEAM_PROPS, {"Infected"})
	
	
end

function GM:PlayerLoadout( pl )
	pl:Give("weapon_hys_knife")
	pl:GiveAmmo(255, "SMG1")
	pl:Give("weapon_smg1")

end

/*function GM:PlayerConnect( name, address ) 
	math.randomseed( os.time() )
    x = math.random(1,100)
	for _, v in pairs(player.GetAll()) do
	//v = player.GetAll()[0]
		x = math.random(1,100)
		Msg("I have no faith in " .. name .. " AKA " .. v:Nick() .. "\n")
		if(v:Team() != 1 and v:Team() != 2) then
			if(team.NumPlayers(TEAM_PROPS) == 0) then
				v:SetTeam(TEAM_PROPS)
			else
				if(x>49) then
					v:SetTeam(TEAM_HUNTERS)
				else
					ptske = team.GetPlayers(TEAM_PROPS)[0]
					ptske:SetTeam(TEAM_HUNTERS)
					v:SetTeam(TEAM_PROPS)
				end
			end
		end
	end
end */



-- Returns the time limit
function GM:GetTimeLimit()

	if (GAMEMODE.GameLength > 0) then
		return GAMEMODE.GameLength * 60;
	end
	
	return -1;
	
end


function util.ToMinutesSeconds(seconds)
	local minutes = math.floor(seconds / 60)
	seconds = seconds - minutes * 60

    return string.format("%02d:%02d", minutes, math.floor(seconds))
end
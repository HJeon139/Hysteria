--
-- init.lua

--

 
-- Send the required lua files to the client.
AddCSLuaFile("vgui/vgui_hudbase.lua")
AddCSLuaFile("vgui/vgui_hudcommon.lua")
AddCSLuaFile("vgui/vgui_hudelement.lua")
AddCSLuaFile("vgui/vgui_hudlayout.lua")
AddCSLuaFile("vgui/vgui_scoreboard.lua")
AddCSLuaFile("vgui/vgui_scoreboard_team.lua")
AddCSLuaFile("vgui/vgui_scoreboard_small.lua")
AddCSLuaFile("cl_skin.lua")
AddCSLuaFile("cl_hud.lua")
AddCSLuaFile("cl_init.lua")
--AddCSLuaFile("cl_scoreboard.lua")
AddCSLuaFile("cl_splashscreen.lua")
AddCSLuaFile("config.lua")
AddCSLuaFile("shared.lua")
AddCSLuaFile("player.lua")


-- If there is a mapfile send it to the client (sometimes servers want to change settings for certain maps).
if file.Exists("../gamemodes/prop_hunt/gamemode/maps/"..game.GetMap()..".lua", "LUA") then

	AddCSLuaFile("maps/"..game.GetMap()..".lua")
	
end


-- Include the required lua files.
include("utility.lua")
include("round_controller.lua")
include("shared.lua")
include("spectator.lua")


-- Make base class available
DEFINE_BASECLASS("gamemode_base")


-- Server only constants.
EXPLOITABLE_DOORS = {
	"func_door",
	"prop_door_rotating", 
	"func_door_rotating"
}

USABLE_PROP_ENTITIES = {
	"prop_physics",
	"prop_physics_multiplayer"
}


-- Send the required resources to the client.
for _, taunt in pairs(HUNTER_TAUNTS) do 
	
	resource.AddFile("sound/"..taunt)
	
end

for _, taunt in pairs(PROP_TAUNTS) do

	resource.AddFile("sound/"..taunt)

end


-- Called when an entity takes damage.
function GM:EntityTakeDamage(target, dmg_info)

	attacker = dmg_info:GetAttacker()

	if GAMEMODE:InRound() && target && target:GetClass() != "ph_prop" && !target:IsPlayer() && attacker && attacker:IsPlayer() && attacker:Team() == TEAM_HUNTERS && attacker:Alive() then
	
		attacker:SetHealth(attacker:Health() - HUNTER_FIRE_PENALTY)
		
		if attacker:Health() <= 0 then
		
			MsgAll(attacker:Name() .. " felt guilty for killing so many innocent people and committed suicide\n")
			attacker:Kill()
			
		end
		
	end
	
end


-- Called when gamemode loads and starts.
function GM:Initialize()
	
	timer.Simple( 3, function() GAMEMODE:StartRoundBasedGame() end )
	game.ConsoleCommand("mp_flashlight 0\n")
	RunConsoleCommand("mp_friendlyfire", "1")
	--RunConsoleCommand( "sb_start" )
	
end


-- Called after all entities have been spawned.
function GM:InitPostEntity()
	
	for _, wep in pairs(ents.FindByClass("weapon_*")) do
	
		wep:Remove()
		
	end
	
	for _, item in pairs(ents.FindByClass("item_*")) do
	
		item:Remove()
		
	end
	
end


-- Called when player tries to pickup a weapon.
function GM:PlayerCanPickupWeapon(pl, ent)

	return true
	
end


-- Called when a player disconnects.
function GM:PlayerDisconnected(pl)
	
	pl:RemoveProp()
	self.BaseClass:PlayerDisconnected(pl)
	
end

function makeInfected()
	local ko = team.GetPlayers(TEAM_HUNTERS)[1]
	ko:SetTeam(TEAM_PROPS)
end
concommand.Add( "sb_infected", makeInfected )

-- Sets the player model
function GM:PlayerSetModel(pl)

	local player_model = "models/Gibs/Antlion_gib_small_3.mdl"
	
	if pl:Team() == TEAM_HUNTERS then
		player_model = "models/player/combine_super_soldier.mdl"
	elseif pl:Team() == TEAM_PROPS then
		player_model = "models/player/combine_super_soldier.mdl" //change to breen for easy debug
	end
	
	util.PrecacheModel(player_model)
	pl:SetModel(player_model)
	
end

-- Called when a player spawns.
function PlayerSpawn(pl)
	
	-- Set the player class based on team
	if pl:Team() == TEAM_HUNTERS then
		player_manager.SetPlayerClass( pl, "player_hunter" )
		pl:PrintMessage( HUD_PRINTTALK, "Welcome human " .. pl:Nick() ) //Gives the message [SimpleBuild]Welcome to the server, (playername here)"  in the talk area.
		--self.BaseClass:PlayerSpawn( pl )
		pl:SetMaxHealth(100, true)
		pl:SetHealth( 100)
	elseif pl:Team() == TEAM_PROPS then
		player_manager.SetPlayerClass( pl, "player_prop" )
		pl:PrintMessage( HUD_PRINTTALK, "You are the infected, " .. pl:Nick() ) //Again, a message in the talk area.
		--self.BaseClass:PlayerSpawn( pl )
		pl:SetMaxHealth(999, true)
		pl:SetHealth( 200)
		Msg(pl:Nick() .. "'s health is " .. pl:Health().. "\n")
	end
		
	
	pl:Blind(false)
	--pl:RemoveProp()
	pl:SetColor(255, 255, 255, 255)
	pl:UnLock()
	pl:ResetHull()
	pl.last_taunt_time = 0
	
	umsg.Start("ResetHull", pl)
	umsg.End()
	--RunConsoleCommand( "sb_start" )
end
hook.Add("PlayerSpawn", "PH_PlayerSpawn", PlayerSpawn)




-- Called when player presses [F3]. Plays a taunt for their team.
function GM:ShowSpare1(pl)

	if GAMEMODE:InRound() && pl:Alive() && (pl:Team() == TEAM_HUNTERS || pl:Team() == TEAM_PROPS) && pl.last_taunt_time + TAUNT_DELAY <= CurTime() && #PROP_TAUNTS > 1 && #HUNTER_TAUNTS > 1 then
	
		-- Select random taunt until it doesn't equal the last taunt.
		repeat
		
			if pl:Team() == TEAM_HUNTERS then
			
				rand_taunt = table.Random(HUNTER_TAUNTS)
				
			else
			
				rand_taunt = table.Random(PROP_TAUNTS)
				
			end
			
		until rand_taunt != pl.last_taunt
		
		pl.last_taunt_time 	= CurTime()
		pl.last_taunt 		= rand_taunt
		
		pl:EmitSound(rand_taunt, 100)
		
	end	
	
end


-- Called every server tick.
function GM:Think()
	--for _, infctd in pairs(team.GetPlayers(TEAM_PROPS))
	--	infctd:SetHealth(infctd:Health()-1)
	--end
		
end


local function SeenSplash( ply )

	if ( ply.m_bSeenSplashScreen ) then return end
	ply.m_bSeenSplashScreen = true
	
	if ( !GAMEMODE.TeamBased && !GAMEMODE.NoAutomaticSpawning ) then
		ply:KillSilent()
	end
	
end

concommand.Add( "seensplash", SeenSplash )
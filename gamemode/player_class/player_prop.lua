
AddCSLuaFile()


DEFINE_BASECLASS("player_default")


-- Create an array to store the player class settings and functions in.
local PLAYER = {}


-- Some settings for the class.
PLAYER.DisplayName			= "Infected"
PLAYER.WalkSpeed 			= 230
PLAYER.CrouchedWalkSpeed 	= 0.2
PLAYER.RunSpeed				= 630
PLAYER.DuckSpeed			= 0.2
PLAYER.DrawTeamRing			= false


-- Called after OnSpawn. Sets the player loadout.
function PLAYER:Loadout()

	--self.Player:GiveAmmo(64, "Buckshot")
	self.Player:GiveAmmo(255, "SMG1")
	
	--self.Player:Give("weapon_shotgun")
	self.Player:Give("weapon_smg1")
	self.Player:Give("item_ar2_grenade")
	
	local cl_defaultweapon = self.Player:GetInfo("cl_defaultweapon") 
 	 
 	if self.Player:HasWeapon(cl_defaultweapon) then 
	
 		self.Player:SelectWeapon(cl_defaultweapon)
	
 	end 
	
end


-- Called when player spawns.
function PLAYER:Spawn()

	local oldhands = self.Player:GetHands();
	
	if (IsValid(oldhands)) then
		
		oldhands:Remove()
		
	end

	local hands = ents.Create( "gmod_hands" )

	if (IsValid(hands)) then
		
		hands:DoSetup(self.Player)
		hands:Spawn()
		
	end	

	
end



-- Called when a player dies.
function PLAYER:Death(attacker, dmginfo)

	self.Player:CreateRagdoll()
	self.Player:Spectate(OBS_MODE_CHASE)
	GAMEMODE: PlayerSpawnAsSpectator( self.Player )
	Msg("No More! Let me die!")
	--self.Player:SetTeam( TEAM_SPECTATOR )
end


-- Register our array of settings and functions as a new class.
player_manager.RegisterClass("player_prop", PLAYER, "player_default")
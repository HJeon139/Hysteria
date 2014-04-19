--
-- player.lua
--


-- Grab a copy of the player meta table.
local meta = FindMetaTable("Player")

-- If there is none, then stop executing this file.
if !meta then return end


-- Blinds the player by setting view out into the void.
function meta:Blind(bool)
	
	-- If the player isn't valid, terminate.
	if !self:IsValid() then 
		
		return 
	
	end
	
	-- If we are on the server then send the blind setting via a usermessage. If we are on client just set the variable.
	if SERVER then
	
		umsg.Start("SetBlind", self)
		umsg.Bool(bool)
		umsg.End()
	
	elseif CLIENT then
	
		blind = bool
		
	end
	
end


-- Returns whether or not the player is a spectator
function meta:IsObserver()
	return ( self:GetObserverMode() > OBS_MODE_NONE );
end

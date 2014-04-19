--
-- cl_init.lua

--
 
 
-- Creating font
function surface.CreateLegacyFont(font, size, weight, antialias, additive, name, shadow, outline, blursize)
	surface.CreateFont(name, {font = font, size = size, weight = weight, antialias = antialias, additive = additive, shadow = shadow, outline = outline, blursize = blursize})
end
 

-- Include the needed files.
include("shared.lua")
include("cl_splashscreen.lua")
include("cl_skin.lua")
include("vgui/vgui_hudlayout.lua")
include("vgui/vgui_hudelement.lua")
include("vgui/vgui_hudbase.lua")
include("vgui/vgui_hudcommon.lua")
include("cl_hud.lua")
include("cl_scoreboard.lua")


-- Fonts!
surface.CreateLegacyFont( "Trebuchet MS", 69, 700, true, false, "FRETTA_HUGE" )
surface.CreateLegacyFont( "Trebuchet MS", 69, 700, true, false, "FRETTA_HUGE_SHADOW", true )
surface.CreateLegacyFont( "Trebuchet MS", 40, 700, true, false, "FRETTA_LARGE" )
surface.CreateLegacyFont( "Trebuchet MS", 40, 700, true, false, "FRETTA_LARGE_SHADOW", true )
surface.CreateLegacyFont( "Trebuchet MS", 19, 700, true, false, "FRETTA_MEDIUM" )
surface.CreateLegacyFont( "Trebuchet MS", 19, 700, true, false, "FRETTA_MEDIUM_SHADOW", true )
surface.CreateLegacyFont( "Trebuchet MS", 16, 700, true, false, "FRETTA_SMALL" )
surface.CreateLegacyFont( "Trebuchet MS", ScreenScale( 10 ), 700, true, false, "FRETTA_NOTIFY", true )

/*function GM:CalcView(pl, origin, angles, fov)
	local view = {}
	view.origin = origin 
 	view.angles	= angles 
 	view.fov 	= fov 
	if (pl:Team() == TEAM_PROPS or pl:Team() == TEAM_HUNTERS) && pl:Alive() then
	
		view.origin = origin + Vector(0, 0, hull_z - 60) + (angles:Forward() * -80)
	end
	return view
end*/



function GM:InitPostEntity()
	
	self.BaseClass:InitPostEntity();
	GAMEMODE:ShowSplash();

end

/*function set_team() //Function for the window when joining as neither special character nor Admin
 
Ready = vgui.Create( "DFrame" ) 				//Define ready as a "DFrame"
Ready:SetPos( ScrW() / 2, ScrH() / 2 ) 			//Set the position. Half the screen height and half the screen width. This will result in being bottom right of the middle of the screen.
Ready:SetSize( 175, 75 ) 						//The size, in pixels of the Frame
Ready:SetTitle( "Welcome to Hysteria" ) //The title; It's at the top.
Ready:SetVisible( true ) 					// Should it be seen? 
Ready:SetDraggable( true ) 				// Can people drag it around?
Ready:SetDeleteOnClose( true )
Ready:ShowCloseButton( false ) 				//Show the little X top right? I chose no, because I have no alternative, meaning people would roam around with no weapons
Ready:MakePopup() 							//Make it popup. Of course.
ready1 = vgui.Create( "DButton", Ready ) 	// Define ready1 as a "DButton" with its parent the Ready frame we just created above.
ready1:SetPos( 20, 25 ) 					//Set position, relative to the frame (If you didn't parent it, it would be relative to the screen
ready1:SetSize( 140, 40 ) 					// How big it should be, again in pixels
ready1:SetText( "Enter" ) 						//What should the button say? 
ready1.DoClick = function() 				//ready1.doclick = function, we just defined it as a function


		if(team.NumPlayers(2) == 0) then
			RunConsoleCommand( "sb_infected" )
		else
			RunConsoleCommand( "sb_humans" )
		end
		//When it clicks, which function does it run? sb_team1, which is defined in init.lua
		Ready:SetVisible( false )
end // end the doclick function
 
end // end the set_team function

concommand.Add( "sb_start", set_team )*/


-- Called immediately after starting the gamemode.
function Initialize()

	hull_z = 80
	surface.CreateFont("ph_arial", { 
		font = "Arial",
		size = 14, 
		weight = 1200, 
		antialias = true,
		shadow = false
	})
	
end
hook.Add("Initialize", "PH_Initialize", Initialize)


-- Resets the player hull.
function ResetHull(um)

	if LocalPlayer() && LocalPlayer():IsValid() then
	
		LocalPlayer():ResetHull()
		hull_z = 80
		
	end
	
end
usermessage.Hook("ResetHull", ResetHull)


-- Sets the local blind variable to be used in CalcView.
function SetBlind(um)

	blind = um:ReadBool()
	
end
usermessage.Hook("SetBlind", SetBlind)


-- Sets the player hull and the health status.
function SetHull(um)

	hull_xy 	= um:ReadLong()
	hull_z 		= um:ReadLong()
	new_health 	= um:ReadLong()
	
	LocalPlayer():SetHull(Vector(hull_xy * -1, hull_xy * -1, 0), Vector(hull_xy, hull_xy, hull_z))
	LocalPlayer():SetHullDuck(Vector(hull_xy * -1, hull_xy * -1, 0), Vector(hull_xy, hull_xy, hull_z))
	LocalPlayer():SetHealth(new_health)
	
end
usermessage.Hook("SetHull", SetHull)


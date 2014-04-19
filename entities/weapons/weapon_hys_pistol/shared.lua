
AddCSLuaFile( "shared.lua" )

SWEP.PrintName		= "Pistol"
--SWEP.WepSelectIcon = "VGUI/hys/icon_pistol"
SWEP.Slot				= 0
SWEP.ViewModelFOV	= 52
SWEP.ViewModelFlip	= false
SWEP.ViewModel		= "models/weapons/v_pistol.mdl"
SWEP.WorldModel		= "models/weapons/w_pistol.mdl"
SWEP.AnimPrefix		= "python"

SWEP.Spawnable			= false
SWEP.AdminSpawnable		= false
SWEP.HoldType = "Pistol"

SWEP.Primary.ClipSize		= 8					// Size of a clip
SWEP.Primary.DefaultClip	= 128				// Default number of bullets in a clip
SWEP.Primary.Automatic		= false				// Automatic/Semi Auto
SWEP.Primary.Ammo			= "Pistol"


/*---------------------------------------------------------
	Initialize
---------------------------------------------------------*/
function SWEP:Initialize()

end


/*---------------------------------------------------------
	Reload
---------------------------------------------------------*/
function SWEP:Reload()
	self:DefaultReload( ACT_VM_RELOAD );
end


/*---------------------------------------------------------
   Think does nothing
---------------------------------------------------------*/
function SWEP:Think()	
end


/*---------------------------------------------------------
	PrimaryAttack
---------------------------------------------------------*/
function SWEP:PrimaryAttack()

	// Make sure we can shoot first
	if ( !self:CanPrimaryAttack() ) then return end

	// Play shoot sound
	self:EmitSound("Weapon_AR2.Single")
	
	// Shoot 9 bullets, 150 damage, 0.01 aimcone
	self:ShootBullet( 150, 1, 0.01 )
	
	// Remove 1 bullet from our clip
	self:TakePrimaryAmmo( 1 )
	
	// Punch the player's view
	self.Owner:ViewPunch( Angle( -1, 0, 0 ) )
	
end

function SWEP:SecondaryAttack()

	// Make sure we can shoot first
	if ( !self:CanPrimaryAttack() ) then return end

	// Play shoot sound
	self:EmitSound("Weapon_AR2.Single")
	
	// Shoot 9 bullets, 150 damage, 0.01 aimcone
	self:ShootBullet( 150, 1, 0.01 )
	
	// Remove 1 bullet from our clip
	self:TakePrimaryAmmo( 1 )
	
	// Punch the player's view
	self.Owner:ViewPunch( Angle( -1, 0, 0 ) )
	
end


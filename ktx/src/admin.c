/*
 * $Id: admin.c,v 1.25 2006/04/18 22:17:17 qqshka Exp $
 */

// admin.c

#include "g_local.h"

void AdminMatchStart();
void StartTimer();
void StopTimer ( int removeDemo );

void NextClient();
void ExitKick(gedict_t *kicker);


void KickThink ()
{
    G_sprint( PROG_TO_EDICT( self->s.v.owner ) , 2, "Your λιγλ mode has timed out\n");
    self->s.v.nextthink = -1;
    ExitKick( PROG_TO_EDICT( self->s.v.owner ) );
}

void AdminKick ()
{
    gedict_t *kickguard;

    if( self->k_captain )
    {
        G_sprint(self, 2, "Exit %s mode first\n", redtext("captain"));
        return;
    }

    if( self->k_admin != 2 )
    {
        G_sprint(self, 2, "You are not an admin\n");
        return;
    }

    if( self->k_kicking )
        ExitKick( self );
    else
    {
        self->k_kicking = 1;
        G_sprint(self, 2, "Kicking process started\n"
						  "\n"
						  "Type ω to kick, ξ for next, λιγλ to leave\n");

        self->kick_ctype = "player";
        self->k_playertokick = world;

        kickguard = spawn(); // Check the 1 minute timeout for kick mode
        kickguard->s.v.owner = EDICT_TO_PROG( self );
        kickguard->s.v.classname = "kickguard";
        kickguard->s.v.think = ( func_t ) KickThink;
        kickguard->s.v.nextthink = g_globalvars.time + 60;

        NextClient();
    }
}

void NextClient ()
{
    float loop = 1;

    while( loop )
    {
		self->k_playertokick = self->k_playertokick ? self->k_playertokick : world;
        self->k_playertokick = find(self->k_playertokick, FOFCLSN, self->kick_ctype);

        if( !(self->k_playertokick) ) {
            if( streq( self->kick_ctype, "player" ) ) {
                self->kick_ctype = "spectator";
				loop++;
			}
            else {
                self->kick_ctype = "player";
				loop++;
			}
		}
        else if( !strnull( self->k_playertokick->s.v.netname )
				&& ( streq ( self->kick_ctype, "spectator" )
						|| ( streq ( self->kick_ctype, "player" )
							 && self->k_playertokick->k_accepted == 2
						   )
				   )
			   )
            loop = 0;
		
		if ( loop > 3 ) {
			G_sprint(self, 2, "Can't find anybody to kick\n");
			ExitKick ( self );
			return;
		}
    }

    G_sprint(self, 2, "Kick %s %s?\n", redtext(self->kick_ctype),
								self->k_playertokick->s.v.netname);
}

void YesKick ()
{
    gedict_t *oldtokick;

    if( self->k_kicking )
    {
        if( !strnull(self->k_playertokick->s.v.classname) && self->k_playertokick != self )
        {
            G_bprint(2, "%s was kicked by %s\n",
								self->k_playertokick->s.v.netname, self->s.v.netname);

            G_sprint(self->k_playertokick, 2, "You were kicked from the server\n");
            oldtokick = self->k_playertokick;

            NextClient();
			
			GhostFlag(oldtokick);
            oldtokick->s.v.classname = "";
            stuffcmd(oldtokick, "disconnect\n"); // FIXME: stupid way
        }
        else if( self->k_playertokick == self )
        {
            G_bprint(2, "%s kicked %s\n", self->s.v.netname, g_himself( self ));

			// hehe %)
            G_sprint(self, 2, "Say \"bye\" and then type \"disconnect\" next time\n");
			GhostFlag(self);
            self->s.v.classname = "";
            stuffcmd(self, "disconnect\n");  // FIXME: stupid way
        }
    }
}

void DontKick ()
{
    if ( self->k_kicking )
        NextClient();
}

void ExitKick (gedict_t *kicker)
{
    gedict_t *childkick;

    if ( kicker->k_kicking )
    {
        kicker->k_kicking = 0;

        childkick = find(world, FOFCLSN, "kickguard");

        while( childkick && PROG_TO_EDICT( childkick->s.v.owner ) != kicker )
            childkick = find(childkick, FOFCLSN, "kickguard");

		if ( childkick )
			ent_remove ( childkick );
		else
			G_Error ("ExitKick null");

        if( !strnull( kicker->s.v.classname ) )
            G_sprint(kicker, 2, "Kicking process terminated\n");
    }
}

void BecomeAdmin(gedict_t *p)
{
	G_bprint(2, "%s %s!\n", p->s.v.netname, redtext("gains admins status"));
	G_sprint(p, 2, "Please give up admin rights when you're done\n"
				   "Type %s for info\n", redtext("commands"));
	p->k_admin = 2;

	on_admin( p );
}

// "admin" command

void ReqAdmin ()
{
    //  admin state=1.5 check for election
    if( self->k_admin == 1.5 )
    {
        G_sprint(self, 2, "Abort %sion first\n", redtext("elect"));
        return;
    }

    if( self->k_admin == 2 )
    {
        G_bprint(2, "%s is no longer an %s\n", self->s.v.netname, redtext("admin"));

        if( self->k_kicking )
            ExitKick( self );

        self->k_admin = 0;

		on_unadmin( self );

        return;
    }

    if( self->k_admin == 1 )
	{
        G_sprint(self, 2, "%s code canceled\n", redtext("admin"));
    	self->k_admin  = 0;
		return;
	}

	if( !cvar( "k_admins" ) ) {
		G_sprint(self, 2, "%s on this server!\n", redtext("NO admins"));
		return;
	}

    if( self->k_admin )
        return;

	if ( Vip_IsFlags( self, VIP_ADMIN ) ) // this VIP does't required pass
    {
		BecomeAdmin(self);
		return;
    }
	
	// parse /admin <pass>
	if (trap_CmdArgc() == 3) {
		char arg_3[1024];
		char *pass = cvar_string( "k_admincode" );

		trap_CmdArgv( 2, arg_3, sizeof( arg_3 ) );

		if ( !strnull(pass) && streq(arg_3, pass) )
			BecomeAdmin(self);
		else
            G_sprint(self, 2, "%s...\n", redtext("Access denied"));

		return;
	}

    self->k_admin  = 1;
    self->k_adminc = 6;
    self->k_added  = 0;

    // You can now use numbers to enter code
    G_sprint(self, 2, "Use %s or %s to enter code\n", redtext("numbers"), redtext("impulses") );
}

void AdminImpBot ()
{
    float coef, i1;

    if( !self->k_adminc )
    {
        self->k_admin = 0;
        return;
    }

    i1 = (int)self->k_adminc - 1;
    coef = self->s.v.impulse;

    while( i1 )
    {
        coef = coef * 10;
        i1--;
    }

    self->k_added  += coef;
    self->k_adminc -= 1;

    if( !self->k_adminc )
    {
		int iPass = cvar( "k_admincode" );

        if( iPass && self->k_added == iPass )
        {
			BecomeAdmin(self);
			return;
        }
        else
        {
            self->k_admin = 0;
            G_sprint(self, 2, "%s...\n", redtext("Access denied"));
        }
    }
    else
        G_sprint(self, 2, "%d %s\n", (int)self->k_adminc, redtext("more to go"));
}

// "ellect" command

void VoteAdmin()
{
	gedict_t *p;
	int   from, till;

	gedict_t *electguard;

// Can't allow election and code entering for the same person at the same time
	if( self->k_admin == 1 ) {
		G_sprint(self, 2, "Finish entering the code first\n");
		return;
	}

	if( self->k_admin == 2 ) {
		G_sprint(self, 2, "You are already an admin\n");
		return;
	}

	if( self->k_admin == 1.5 ) {
		G_bprint(2, "%s %s!\n", self->s.v.netname, redtext("aborts election"));
		AbortElect();
		return;
	}

// Only one election per server because otherwise we wouldn't know how to count
// "yes"s or "no"s
	if( get_votes( OV_ELECT ) ) {
		G_sprint(self, 2, "An election is already in progress\n");
		return;
	}

	if( !cvar( "k_admins" ) ) {
		G_sprint(self, 2, "%s on this server!\n", redtext("NO admins"));
		return;
	}

// Check if voteadmin is allowed
	if( !cvar( "k_allowvoteadmin" ) ) {
		G_sprint(self, 2, "Admin election is not allowed on this server\n");
		return;
	}

	if( (till = Q_rint( self->v.elect_block_till - g_globalvars.time)) > 0  ) {
		G_sprint(self, 2, "Wait %d second%s!\n", till, count_s(till) );
		return;
	}

	if( streq( self->s.v.classname, "spectator" ) && match_in_progress )
		return;

	G_bprint(2, "%s has %s rights!\n", self->s.v.netname, redtext("requested admin"));

	for( from = 0, p = world; p = find_plrspc(p, &from); )
		if ( p != self && p->k_player )
			G_sprint(p, 2, "Type %s in console to approve\n", redtext("yes"));

	G_sprint(self, 2, "Type %s to abort election\n", redtext("elect"));

    // announce the election
	self->v.elect = 1;

	self->k_admin = 1.5;

	electguard = spawn(); // Check the 1 minute timeout for election
	electguard->s.v.owner = EDICT_TO_PROG( world );
	electguard->s.v.classname = "electguard";
	electguard->s.v.think = ( func_t ) ElectThink;
	electguard->s.v.nextthink = g_globalvars.time + 60;
}

void AdminMatchStart ()
{
    gedict_t *p;
    int i = 0;

    for( p = world; p = find(p, FOFCLSN, "player"); )
    {
		if( p->ready && p->k_accepted == 2 ) {
			i++;
		}
		else
		{
			G_bprint(2, "%s was kicked by admin forcestart\n", p->s.v.netname);
			G_sprint(p, 2, "Bye bye! Pay attention next time.\n");

			p->k_accepted = 0;
			p->s.v.classname = "";
			stuffcmd(p, "disconnect\n"); // FIXME: stupid way
		}
	}

    k_attendees = i;

    if( k_attendees ) {
        StartTimer();
	}
    else
    {
        G_bprint(2, "Can't start! More players needed.\n");
		EndMatch( 1 );
    }
}

void ReadyThink ()
{
    float i1;
	char *txt, *gr;
    gedict_t *p=NULL, *p2=NULL;

    p2 = PROG_TO_EDICT( self->s.v.owner );
	
    if(    ( p2->k_player && !( p2->ready ) ) // forcestart breaked via break command
		|| ( p2->k_spectator && !k_force )	// forcestart breaked via forcebreak command (spectator admin)
	  )
    {
        k_force = 0;

        G_bprint(2, "%s interrupts countdown\n", p2->s.v.netname );

        ent_remove ( self );

        return;
    }

	k_attendees = CountPlayers();

	if ( !isCanStart(NULL, true) ) {
        k_force = 0;

        G_bprint(2, "Forcestart canceled\n");

        ent_remove ( self );

        return;
	}


    self->attack_finished--;

    i1 = self->attack_finished;

    if( i1 <= 0 )
    {
        k_force = 0;

        AdminMatchStart();

        ent_remove ( self );

        return;
    }

	txt = va( "%s second%s to gamestart", dig3( i1 ), ( i1 == 1 ? "" : "s") );
	gr  = va( "\n%s!", redtext("Go ready") );

    for( p = world; p = find(p, FOFCLSN, "player"); )
		G_centerprint(p, "%s%s", txt, (p->ready ? "" : gr));

    for( p = world; p = find(p, FOFCLSN, "spectator"); )
		G_centerprint(p, "%s", txt);

    self->s.v.nextthink = g_globalvars.time + 1;
}

void AdminForceStart ()
{
    gedict_t *mess;

    if( match_in_progress || self->k_admin != 2 )
        return;

	// no forcestart in practice mode
	if ( k_practice ) {
		G_sprint(self, 2, "%s\n", redtext("Server in practice mode"));
		return;
	}

    if( streq( self->s.v.classname, "player" ) && !self->ready )
    {
        G_sprint(self, 2, "Ready yourself first\n");
        return;
    }

	k_attendees = CountPlayers();

	if ( !isCanStart( self, true ) ) {
        G_sprint(self, 2, "Can't issue!\n");
		return;
	}

    if( k_attendees )
    {
        G_bprint(2, "%s forces matchstart!\n", self->s.v.netname);

        k_force = 1;

        mess = spawn();
        mess->s.v.classname = "mess";
        mess->s.v.owner = EDICT_TO_PROG( self );
        mess->s.v.think = ( func_t ) ReadyThink;
        mess->s.v.nextthink = g_globalvars.time + 0.1;
        mess->attack_finished = 10 + 1;
    }
    else
        G_sprint(self, 2, "Can't issue! More players needed.\n");
}


void AdminForceBreak ()
{
//    char *tmp;

    if( self->k_admin == 2 && strneq( self->s.v.classname, "player" ) && !match_in_progress )
    {
        k_force = 0;
        return;
    }

    if( self->k_admin != 2 || !match_in_progress )
        return;

    if( strneq( self->s.v.classname, "player" ) && match_in_progress == 1 )
    {
        k_force = 0;
        StopTimer( 1 );

        return;
    }

    if( k_oldmaxspeed ) // huh???
        cvar_fset( "sv_maxspeed", k_oldmaxspeed );

    G_cprint("%%forcebreak%%%s\n", self->s.v.netname);
    G_bprint(2, "%s forces a break!\n"
				"MATCH OVER!!\n", self->s.v.netname);

    EndMatch( 0 );
}

void TogglePreWar ()
{
    gedict_t *p = world;
    float tmp;

    if( self->k_admin != 2 )
        return;

    tmp = cvar( "k_prewar" );

    if( tmp )
    {
		cvar_fset( "k_prewar", 0 );

        if( !match_in_progress )
        {
            G_bprint(2, "Players may ξοτ fire before match!\n");

            p = find(p, FOFCLSN, "player");
            while( p )
            {
                if( !strnull ( p->s.v.netname ) )
                    stuffcmd(p, "-attack\n");

                p = find(p, FOFCLSN, "player");
            }
        }
        else
            G_sprint(self, 2, "Players may ξοτ fire before match!\n");

        return;
    }

	cvar_fset( "k_prewar", 1 );

    if( !match_in_progress )
        G_bprint(2, "Players may fire before match.\n");
    else
        G_sprint(self, 2, "Players may fire before match.\n");
}

void ToggleMapLock ()
{
    float tmp;

    if( self->k_admin != 2 )
        return;

    tmp = cvar( "k_lockmap" );

    if( tmp )
    {
		cvar_fset( "k_lockmap", 0 );

        if( !match_in_progress )
            G_bprint(2, "%s unlocks map\n", self->s.v.netname);
        else
            G_sprint(self, 2, "Map unlocked\n");

        return;
    }

	cvar_fset( "k_lockmap", 1 );

    if( !match_in_progress )
        G_bprint(2, "%s locks map\n", self->s.v.netname);
    else
        G_sprint(self, 2, "Map is locked\n");
}

void ToggleMaster ()
{
    float f1;

    if( self->k_admin != 2 )
        return;

    f1 = !cvar( "k_master" );

    if( f1 )
        G_bprint(2, "%s sets mastermode!\n"
					"Players may %s alter settings\n", self->s.v.netname, redtext("not"));
    else
        G_bprint(2, "Mastermode disabled by %s\n"
					"Players %s now alter settings\n", self->s.v.netname, redtext("can"));

	cvar_fset( "k_master", f1 );
}


// FIXME: and why we can't use server pause command, instead of this UGLY HACK?
// may be because pause command dosn't exist? %)

int k_realPausable = 0;

void ModPause (int pause)
{
    gedict_t *e1;
    float f1;

	if ( k_pause == pause )
		G_Error ("ModPause: k_pause == pause");

    k_pause = pause;

    WriteByte(MSG_ALL, SVC_SETPAUSE);

    if( pause )
    {
        k_pausetime = g_globalvars.time;

		k_realPausable = (int) cvar( "pausable" ); // save
        cvar_set("pausable", "0"); // mod pause issued, so server native pause can't be issued

        k_oldmaxspeed = k_maxspeed;
        cvar_set("sv_maxspeed", "0");

        WriteByte(MSG_ALL, 1); // pause plaque on

        e1 = nextent( world );
        while ( e1 )
        {
            if( e1->s.v.nextthink > g_globalvars.time )
            {
                e1->k_ptime = e1->s.v.nextthink;
                e1->s.v.nextthink = -1;

                if( streq( e1->s.v.classname, "player" ) )
                    e1->maxspeed = 0;
            }

            e1 = nextent( e1 );
        }
    }
    else
    {
        WriteByte(MSG_ALL, 0); // pause plaque off

        cvar_set("pausable", va("%d", k_realPausable)); // restore
        cvar_set("sv_maxspeed", va("%d", (int)k_oldmaxspeed));

        f1 = g_globalvars.time - k_pausetime;

        e1 = nextent( world );
        while( e1 )
        {
            if(e1->s.v.nextthink == -1)
                e1->s.v.nextthink = f1 + e1->k_ptime;
            if( streq( e1->s.v.classname, "player" ) )
            {
                e1->maxspeed = k_oldmaxspeed;
                e1->pain_finished += f1;

                if(e1->invincible_finished)
// qqshka: imho wrong   e1->invincible_finished = f1 + e1->pain_finished;
// FIXME: check this
				e1->invincible_finished			+= f1;

                if(e1->invisible_finished)
                    e1->invisible_finished		+= f1;

                if(e1->super_damage_finished)
                    e1->super_damage_finished	+= f1;

                if(e1->super_damage_finished)
                    e1->radsuit_finished		+= f1;

                e1->air_finished += f1;
            }
            else if(   streq( e1->s.v.classname, "dropring" )
					|| streq( e1->s.v.classname, "dropquad" ) )
                e1->cnt += f1;

            e1 = nextent( e1 );
        }
    }
}

void AdminForcePause ()
{
    if( self->k_admin != 2 || match_in_progress != 2 )
        return;

    if( !k_pause )
        G_bprint(2, "%s issues a pause\n", self->s.v.netname);
    else
        G_bprint(2, "unpaused by %s!\n", self->s.v.netname);

	ModPause (!k_pause);
}

// qqshka

void ToggleFallBunny ()
{
    if( self->k_admin != 2 )
        return;

    if( match_in_progress )
        return;

	cvar_toggle_msg( self, "k_fallbunny", redtext("fallbunny") );
}


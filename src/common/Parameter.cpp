//-------------------------------------------------------------------------------------------------------
//			Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "SurgeStorage.h"
#include "Parameter.h"
#include "DspUtilities.h"
#include <string.h>
#include <math.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <UserDefaults.h>

Parameter::Parameter() : posx( PositionHolder::Axis::X ),
                         posy( PositionHolder::Axis::Y ),
                         posy_offset( PositionHolder::Axis::YOFF )
{
   val.i = 0;
   posy_offset = 0;
   storage = nullptr;
}

Parameter::~Parameter()
{
}

void get_prefix(char* txt, ControlGroup ctrlgroup, int ctrlgroup_entry, int scene)
{
   char prefix[19];
   switch (ctrlgroup)
   {
   case cg_OSC:
      sprintf(prefix, "osc%i_", ctrlgroup_entry + 1);
      break;
   case cg_FILTER:
      sprintf(prefix, "filter%i_", ctrlgroup_entry + 1);
      break;
   case cg_ENV:
      sprintf(prefix, "env%i_", ctrlgroup_entry + 1);
      break;
   /*case 6:
           sprintf(prefix,"ms%i_",ctrlgroup_entry+1);
           break;*/
   case cg_FX:
      sprintf(prefix, "fx%i_", ctrlgroup_entry + 1);
      break;
   default:
      prefix[0] = '\0';
      break;
   };
   if (scene == 2)
      sprintf(txt, "b_%s", prefix);
   else if (scene == 1)
      sprintf(txt, "a_%s", prefix);
   else
      sprintf(txt, "%s", prefix);
}

void create_fullname(char* dn, char* fn, ControlGroup ctrlgroup, int ctrlgroup_entry)
{
   char prefix[32];
   bool useprefix = true;
   switch (ctrlgroup)
   {
   case cg_OSC:
      sprintf(prefix, "Osc %i", ctrlgroup_entry + 1);
      break;
   case cg_FILTER:
      sprintf(prefix, "Filter %i", ctrlgroup_entry + 1);
      break;
   case cg_ENV:
      if (ctrlgroup_entry)
         sprintf(prefix, "FEG");
      else
         sprintf(prefix, "AEG");
      break;
   case cg_LFO:
   {
      int a = ctrlgroup_entry + 1 - ms_lfo1;
      if (a > 6)
         sprintf(prefix, "Scene LFO %i", a - 6);
      else
         sprintf(prefix, "LFO %i", a);
   }
   break;
   case cg_FX:
      switch( ctrlgroup_entry )
      {
      case 0:
         sprintf( prefix, "FX A1" );
         break;
      case 1:
         sprintf( prefix, "FX A2" );
         break;
      case 2:
         sprintf( prefix, "FX B1" );
         break;
      case 3:
         sprintf( prefix, "FX B2" );
         break;
      case 4:
         sprintf( prefix, "FX S1" );
         break;
      case 5:
         sprintf( prefix, "FX S2" );
         break;
      case 6:
         sprintf( prefix, "FX M1" );
         break;
      case 7:
         sprintf( prefix, "FX M2" );
         break;
      default:
         sprintf( prefix, "FXERR" );
         break;
      }
      break;
   default:
      prefix[0] = '\0';
      useprefix = false;
      break;
   };

   char tfn[256];
   if (useprefix)
      sprintf(tfn, "%s %s", prefix, dn);
   else
      sprintf(tfn, "%s", dn);
   memset(fn, 0, NAMECHARS * sizeof(char));
   strncpy(fn, tfn, NAMECHARS - 1 );
}

void Parameter::set_name(const char* n)
{
   strncpy(dispname, n, NAMECHARS);
   create_fullname(dispname, fullname, ctrlgroup, ctrlgroup_entry);
}

Parameter* Parameter::assign(ParameterIDCounter::promise_t idp,
                             int pid,
                             const char* name,
                             const char* dispname,
                             int ctrltype,

                             std::string ui_identifier,
                             int posx,
                             int posy,
                             int scene,
                             ControlGroup ctrlgroup,
                             int ctrlgroup_entry,
                             bool modulateable,
                             int ctrlstyle)
{
   this->id_promise = idp.get();
   this->id = -1;
   this->param_id_in_scene = pid;
   this->ctrlgroup = ctrlgroup;
   this->ctrlgroup_entry = ctrlgroup_entry;
   this->posx = posx;
   this->posy = posy;
   this->modulateable = modulateable;
   this->scene = scene;
   this->ctrlstyle = ctrlstyle;
   this->storage = nullptr;
   strncpy(this->ui_identifier, ui_identifier.c_str(), NAMECHARS );

   strncpy(this->name, name, NAMECHARS);
   set_name(dispname);
   char prefix[16];
   get_prefix(prefix, ctrlgroup, ctrlgroup_entry, scene);
   sprintf(name_storage, "%s%s", prefix, name);
   posy_offset = 0;
   if (scene)
      per_voice_processing = true;
   else
      per_voice_processing = false;
   clear_flags();
   midictrl = -1;

   set_type(ctrltype);
   if (valtype == vt_float)
      val.f = val_default.f;

   bound_value();
   return this;
}

void Parameter::clear_flags()
{
   temposync = false;
   extend_range = false;
   absolute = false;
}

bool Parameter::can_temposync()
{
   switch (ctrltype)
   {
   case ct_portatime:
   case ct_lforate:
   case ct_envtime:
   case ct_envtime_lfodecay:
      return true;
   }
   return false;
}

bool Parameter::can_extend_range()
{
   switch (ctrltype)
   {
   case ct_pitch_semi7bp:
   case ct_pitch_semi7bp_absolutable:
   case ct_freq_shift:
   case ct_decibel_extendable:
   case ct_decibel_narrow_extendable:
   case ct_oscspread:
   case ct_osc_feedback:
      return true;
   }
   return false;
}

bool Parameter::can_be_absolute()
{
   switch (ctrltype)
   {
   case ct_oscspread:
   case ct_pitch_semi7bp_absolutable:
      return true;
   }
   return false;
}

void Parameter::set_user_data(ParamUserData* ud)
{
   switch (ctrltype)
   {
   case ct_countedset_percent:
      if (dynamic_cast<CountedSetUserData*>(ud))
      {
         user_data = ud;
      }
      else
      {
         user_data = nullptr;
      }
      break;
   default:
      std::cerr << "Setting userdata on a non-supporting param ignored" << std::endl;
      user_data = nullptr;
      break;
   }
}

void Parameter::set_type(int ctrltype)
{
   this->ctrltype = ctrltype;
   this->posy_offset = 0;
   this->moverate = 1.f;

   affect_other_parameters = false;
   user_data = nullptr;
   switch (ctrltype)
   {
   case ct_pitch:
      valtype = vt_float;
      val_min.f = -60;
      val_max.f = 60;
      val_default.f = 0;
      break;
   case ct_syncpitch:
      valtype = vt_float;
      val_min.f = 0;
      val_max.f = 60;
      val_default.f = 0;
      break;
   case ct_fmratio:
      valtype = vt_float;
      val_min.f = 0;
      val_max.f = 32;
      val_default.f = 1;
      this->moverate = 0.25f;
      break;
   case ct_fmratio_int:
      valtype = vt_int;
      val_min.i = 1;
      val_max.i = 32;
      val_default.i = 1;
      break;
   case ct_pbdepth:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = 24;
      val_default.i = 2;
      break;
   case ct_pitch_semi7bp:
   case ct_pitch_semi7bp_absolutable:
      valtype = vt_float;
      val_min.f = -7;
      val_max.f = 7;
      moverate = 0.5f;
      val_default.f = 0;
      break;
   case ct_freq_audible:
      valtype = vt_float;
      val_min.f = -60;
      val_max.f = 70;
      val_default.f = 3;
      // val_max.f = 76;
      break;
   case ct_freq_hpf:
      valtype = vt_float;
      val_min.f = -72;
      val_max.f = 15;
      val_default.f = -18;
      break;
   case ct_freq_vocoder_low:
      valtype = vt_float;
      val_min.f = -36; // 55hz
      val_max.f = 36; // 3520 hz
      val_default.f = -3;
      break;
   case ct_freq_vocoder_high:
      valtype = vt_float;
      val_min.f = 0; // 440 hz
      val_max.f = 60; // ~14.3 khz
      val_default.f = 49; // ~7.4khz
      break;
   case ct_freq_mod:
      valtype = vt_float;
      val_min.f = -96;
      val_max.f = 96;
      val_default.f = 0;
      moverate = 0.5f;
      break;
   case ct_freq_shift:
      valtype = vt_float;
      val_min.f = -10;
      val_max.f = 10;
      val_default.f = 0;
      break;
   case ct_bandwidth:
      valtype = vt_float;
      val_min.f = 0;
      val_max.f = 5;
      val_default.f = 1;
      break;
   case ct_decibel:
   case ct_decibel_extendable:
      valtype = vt_float;
      val_min.f = -48;
      val_max.f = 48;
      val_default.f = 0;
      break;
   case ct_decibel_attenuation:
      valtype = vt_float;
      val_min.f = -48;
      val_max.f = 0;
      val_default.f = 0;
      break;
   case ct_decibel_attenuation_large:
      valtype = vt_float;
      val_min.f = -96;
      val_max.f = 0;
      val_default.f = 0;
      break;
   case ct_decibel_fmdepth:
      valtype = vt_float;
      val_min.f = -48;
      val_max.f = 24;
      val_default.f = 0;
      break;
   case ct_decibel_narrow:
   case ct_decibel_narrow_extendable:
      valtype = vt_float;
      val_min.f = -24;
      val_max.f = 24;
      val_default.f = 0;
      break;
   case ct_decibel_extra_narrow:
      valtype = vt_float;
      val_min.f = -12;
      val_max.f = 12;
      val_default.f = 0;
      break;
   case ct_portatime:
      valtype = vt_float;
      val_min.f = -8;
      val_max.f = 2;
      val_default.f = -8;
      break;
   case ct_envtime:
   case ct_envtime_lfodecay:
      valtype = vt_float;
      val_min.f = -8;
      val_max.f = 5;
      val_default.f = 0;
      break;
   case ct_delaymodtime:
   case ct_chorusmodtime:
      valtype = vt_float;
      val_min.f = -11;
      val_max.f = -3;
      val_default.f = -6;
      break;
   case ct_reverbtime:
      valtype = vt_float;
      val_min.f = -4;
      val_max.f = 6;
      val_default.f = 1;
      break;
   case ct_reverbpredelaytime:
      valtype = vt_float;
      val_min.f = -4;
      val_max.f = 1;
      val_default.f = -2;
      break;
   case ct_lforate:
      valtype = vt_float;
      val_min.f = -7;
      val_max.f = 9;
      val_default.f = 0;
      moverate = 0.33f;
      break;
   case ct_lfotrigmode:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = n_lfomodes - 1;
      val_default.i = 0;
      break;
   case ct_pitch_octave:
      valtype = vt_int;
      val_min.i = -3;
      val_max.i = 3;
      val_default.i = 0;
      break;
   case ct_bool_mute:
   case ct_bool_solo:
   case ct_bool_fm:
   case ct_bool_keytrack:
   case ct_bool_retrigger:
   case ct_bool_relative_switch:
   case ct_bool_link_switch:
   case ct_bool_unipolar:
   case ct_bool:
      valtype = vt_bool;
      val_min.i = 0;
      val_max.i = 1;
      val_default.i = 0;
      break;
   case ct_osctype:
      valtype = vt_int;
      val_min.i = 0;
      val_default.i = 0;
      val_max.i = num_osctypes - 1;
      affect_other_parameters = true;
      break;
   case ct_reverbshape:
      valtype = vt_int;
      val_min.i = 0;
      val_default.i = 0;
      val_max.i = 3;
      break;
   case ct_fxtype:
      valtype = vt_int;
      val_min.i = 0;
      val_default.i = 0;
      val_max.i = num_fxtypes - 1;
      // affect_other_parameters = true;	// Can not be added, before it has a custom
      // controltype
      break;
   case ct_fxbypass:
      valtype = vt_int;
      val_min.i = 0;
      val_default.i = 0;
      val_max.i = n_fx_bypass - 1;
      break;
   case ct_oscroute:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = 2;
      val_default.i = 1;
      break;
   case ct_envshape:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = 2;
      val_default.i = 0;
      break;
   case ct_envmode:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = 1;
      val_default.i = 0;
      break;
   case ct_stereowidth:
      valtype = vt_float;
      val_min.f = 0;
      val_max.f = 120;
      val_default.f = 90;
      break;
   case ct_lfoshape:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = n_lfoshapes - 1;
      val_default.i = 0;
      break;
   case ct_fbconfig:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = n_fb_configuration - 1;
      val_default.i = 0;
      break;
   case ct_fmconfig:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = n_fm_configuration - 1;
      val_default.i = 0;
      break;
   case ct_filtertype:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = n_fu_type - 1;
      val_default.i = 0;
      break;
   case ct_filtersubtype:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = 3;
      val_default.i = 0;
      break;
   case ct_wstype:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = n_ws_type - 1; 
      val_default.i = 0;
      break;
   case ct_midikey_or_channel:
   case ct_midikey:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = 127;
      val_default.i = 60;
      break;
   case ct_wt2window:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = 8;
      val_default.i = 0;
      break;
   case ct_osccount:
   case ct_osccountWT:
      valtype = vt_int;
      val_min.i = 1;
      val_max.i = 16;
      val_default.i = 1;
      break;
   case ct_scenemode:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = n_scenemodes - 1;
      val_default.i = 0;
      break;
   case ct_polymode:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = n_polymodes - 1;
      val_default.i = 0;
      break;
   case ct_polylimit:
      valtype = vt_int;
      val_min.i = 2;
      val_max.i = 64;
      val_default.i = 8;
      break;
   case ct_scenesel:
      valtype = vt_int;
      val_min.i = 0;
      val_max.i = 1;
      val_default.i = 0;
      break;
   case ct_percent:
      val_min.f = 0;
      val_max.f = 1;
      valtype = vt_float;
      val_default.f = 0;
      break;
   case ct_oscspread:
      val_min.f = 0.f;
      val_max.f = 1.f;
      valtype = vt_float;
      val_default.f = 0.2;
      break;
   case ct_detuning:
      val_min.f = 0;
      val_max.f = 2;
      valtype = vt_float;
      val_default.f = 0;
      break;
   case ct_amplitude:
      val_min.f = 0;
      val_max.f = 1;
      valtype = vt_float;
      val_default.f = 1;
      break;
   case ct_percent_bidirectional:
      val_min.f = -1;
      val_max.f = 1;
      valtype = vt_float;
      val_default.f = 0;
      break;
   case ct_character:
      val_min.i = 0;
      val_max.i = 2;
      valtype = vt_int;
      val_default.i = 1;
      break;
   case ct_sineoscmode:
      val_min.i = 0;
      val_max.i = 19;
      valtype = vt_int;
      val_default.i = 0;
      break;
   case ct_sinefmlegacy:
      val_min.i = 0;
      val_max.i = 1;
      valtype = vt_int;
      val_default.i = 0;
      break;
   case ct_vocoder_bandcount:
      val_min.i = 4;
      val_max.i = 20;
      valtype = vt_int;
      val_default.i = 20;
      break;
   case ct_distortion_waveshape:
      val_min.i = 0;
      val_max.i = n_ws_type - 2; // we want to skip none also
      valtype = vt_int;
      val_default.i = 0;
      break;
   case ct_countedset_percent:
      val_min.f = 0;
      val_max.f = 1;
      valtype = vt_float;
      val_default.f = 0;
      break;
   case ct_flangerpitch:
      val_min.f = 0;
      val_max.f = 127;
      valtype = vt_float;
      val_default.f = 60;
      break;
   case ct_flangervoices:
      val_min.f = 1;
      val_max.f = 4;
      valtype = vt_float;
      val_default.f = 4;
      break;
   case ct_flangermode:
      val_min.i = 0;
      val_max.i = 5; // classic, classic tuned, doppler, doppler tuned, arpeggio blend, arpeggio bare
      valtype = vt_int;
      val_default.i = 0;
      break;
   case ct_flangerwave:
      val_min.i = 0;
      val_max.i = 3; // sin, tri, saw, s&h
      val_default.i = 0;
      break;
   case ct_flangerchord:
      val_min.i = 0;
      val_max.i = 12;
      valtype = vt_int;
      val_default.i = 0;
      break;
   case ct_osc_feedback:
      val_min.f = 0;
      val_max.f = 1;
      valtype = vt_float;
      val_default.f = 0;
      break;
   default:
   case ct_none:
      sprintf(dispname, "-");
      valtype = vt_int;

      val_min.i = std::numeric_limits<int>::min();
      val_max.i = std::numeric_limits<int>::max();
      val_default.i = 0;
      break;
   }
}

void Parameter::bound_value(bool force_integer)
{
   if (temposync && (valtype == vt_float))
   {
      float a, b = modff(val.f, &a);
      if (b < 0)
      {
         b += 1.f;
         a -= 1.f;
      }
      b = powf(2.0f, b);

      if (b > 1.41f)
      {
         b = log2(1.5f);
      }
      else if (b > 1.167f)
      {
         b = log2(1.3333333333f);
      }
      else
      {
         b = 0.f;
      }

      val.f = a + b;
   }

   if (force_integer && (valtype == vt_float))
   {
      switch (ctrltype)
      {
      case ct_percent:
      case ct_percent_bidirectional:
      case ct_osc_feedback:
      case ct_detuning:
      {
         val.f = floor(val.f * 100) / 100.0;
         break;
      }
      case ct_pitch:
      case ct_pitch_semi7bp:
      case ct_syncpitch:
      {
         if (!extend_range) {
            val.f = floor(val.f + 0.5f); }

         break;
      }
      case ct_oscspread:
      {
         if (absolute)
         {
            if (extend_range) {
               val.f = floor(val.f * 192) / 192.0; }
            else {
               val.f = floor(val.f * 16) / 16.0; }
         }
         else if (extend_range)
         {
            val.f = floor(val.f * 120) / 120.0;
         }
         else
         {
            val.f = floor(val.f * 100) / 100.0;
         }

         break;
      }
      case ct_pitch_semi7bp_absolutable:
      {
         if (absolute)
            if (extend_range)
            {
               val.f = floor(val.f * 120) / 120.0;
            }
            else
            {
               val.f = floor(val.f * 12) / 12.0;
            }
         else
         {
            val.f = floor(val.f + 0.5f);
         }
         break;
      }
      case ct_amplitude:
      {
         if (val.f != 0)
         {
            val.f = db_to_amp(
                round(amp_to_db(val.f))); // we use round instead of floor because with some params
                                          // it wouldn't snap to max value (i.e. send levels)
         }
         else
         {
            val.f = -INFINITY; // this is so that the popup shows -inf proper instead of -192.0
         }
         break;
      }
      case ct_decibel:
      case ct_decibel_narrow:
      case ct_decibel_narrow_extendable:
      case ct_decibel_extra_narrow:
      case ct_decibel_attenuation:
      case ct_decibel_attenuation_large:
      case ct_decibel_fmdepth:
      case ct_decibel_extendable:
      {
         val.f = floor(val.f);
         break;
      }
      case ct_chorusmodtime:
      {
         val.f = limit_range(
                    (float) log2(round(powf(2.0f, val.f) * 100) / 100.f),
                            val_min.f, val_max.f);
         break;
      }
      case ct_portatime:
      case ct_envtime:
      case ct_envtime_lfodecay:
      case ct_reverbtime:
      case ct_reverbpredelaytime:
      case ct_delaymodtime:
         if (temposync)
         {
             val.f = floor(val.f + 0.5f);
         }
         else
         {
            val.f = log2(round(powf(2.0f, val.f) * 10) / 10.f);
         }
         break;
      case ct_lforate:
      {
         if (temposync)
         {
            val.f = floor(val.f + 0.5f);
         }
         else if (val.f < 0)
         {
            val.f = limit_range(
                      (float) log2(round(powf(2.0f, val.f) * 10) / 10.f),
                               val_min.f, val_max.f);
         }
         else
         {
            val.f = log2(round(powf(2.0f, val.f)));
         }

         break;
      }
      case ct_bandwidth:
      {
         val.f = floor(val.f * 10) / 10.0;
         break;
      }
      case ct_freq_shift:
      {
         if (extend_range) {
            val.f = floor(val.f * 100) / 100.0; }
         else {
            val.f = floor(val.f + 0.5f); }
         break;
      }
      case ct_countedset_percent:
      {
         CountedSetUserData* cs = reinterpret_cast<CountedSetUserData*>(user_data);
         auto count = cs->getCountedSetSize();
         // OK so now val.f is between 0 and 1. So
         auto fraccount = val.f * count;
         auto intcount = (int)fraccount;
         val.f = 1.0 * intcount / count;
         break;
      }
      default: {
         val.f = floor(val.f + 0.5f);
         break; }
      }
   }

   if (ctrltype == ct_vocoder_bandcount) {
       val.i = val.i - val.i % 4; }
   
   switch (valtype)
   {
   case vt_float: {
      val.f = limit_range(val.f, val_min.f, val_max.f);
      break; }
   case vt_int: {
      val.i = limit_range(val.i, val_min.i, val_max.i);
      break; }
   };
}

const char* Parameter::get_name()
{
   return dispname;
}

const char* Parameter::get_full_name()
{
   return fullname;
}

const char* Parameter::get_internal_name()
{
   return name;
}

const char* Parameter::get_storage_name()
{
   return name_storage;
}

char* Parameter::get_storage_value(char* str)
{
   switch (valtype)
   {
   case vt_int:
      sprintf(str, "%i", val.i);
      break;
   case vt_bool:
      sprintf(str, "%i", val.b ? 1 : 0);
      break;
   case vt_float:
      std::stringstream sst;
      sst.imbue(std::locale::classic());
      sst << std::fixed;
      sst << std::showpoint;
      sst << std::setprecision(6);
      sst << val.f;
      strncpy(str, sst.str().c_str(),15);
      break;
   };

   return str;
}

void Parameter::set_storage_value(int i)
{
   switch (ctrltype)
   {
   default: {
      val.i = i;
      break; }
   }
}
void Parameter::set_storage_value(float f)
{
   switch (ctrltype)
   {
   /*case ct_amplitude:
           val.f = db_to_amp(f);
           break;*/
   default: {
      val.f = f;
      break; }
   }
}

float Parameter::get_extended(float f)
{
   if (!extend_range)
      return f;

   switch (ctrltype)
   {
   case ct_freq_shift:
      return 100.f * f;
   case ct_pitch_semi7bp:
   case ct_pitch_semi7bp_absolutable:
      return 12.f * f;
   case ct_decibel_extendable:
      return 3.f * f;
   case ct_decibel_narrow_extendable:
      return 5.f * f;
   case ct_oscspread:
      return 12.f * f;
   case ct_osc_feedback:
      return 8.f * f - 4.f;
   default:
      return f;
   }
}

std::string Parameter::tempoSyncNotationValue(float f)
{
    float a, b = modff(f, &a);
    
    if (b >= 0)
    {
        b -= 1.0;
        a += 1.0;
    }

    float d, q;
    std::string nn, t;
    char tmp[1024];
    
    if(f >= 1)
    {
        q = pow(2.0, f - 1);
        nn = "whole";
        if (q >= 3)
        {
           if( abs( q - floor(q + 0.01 ) ) < 0.01 )
           {
              snprintf(tmp, 1024, "%d whole notes", (int)floor(q + 0.01));
           }
           else
           {
              // this is the triplet case
              snprintf(tmp, 1024, "%d whole triplets", (int)floor(q * 3.0 / 2.0 + 0.02 ));
           }
           std::string res = tmp;
           return res;
        }
        else if (q >= 2)
        {
            nn = "double whole";
            q /= 2;
        }

        if (q < 1.3)
        {
            t = "note";
        }
        else if (q < 1.4)
        {
            t = "triplet";
            if (nn == "whole")
            {
               nn = "double whole";
            }
            else
            {
               q = pow(2.0, f - 1);
               snprintf(tmp, 1024, "%d whole triplets", (int)floor(q * 3.0 / 2.0 + 0.02 ));
               std::string res = tmp;
               return res;
            }
        }
        else
        {
            t = "dotted";
        }
    }
    else
    {
        d = pow(2.0, -(a - 2));
        q = pow(2.0, (b+1));

        if (q < 1.3)
        {
            t = "note";
        }
        else if (q < 1.4)
        {
            t = "triplet";
            d = d / 2;
        }
        else
        {
            t = "dotted";
        }
        if (d == 1)
        {
            nn = "whole";
        }
        else
        {
            char tmp[1024];
            snprintf(tmp, 1024, "1/%d", (int)d );
            nn = tmp;
        }
    }
    std::string res = nn + " " + t;

    return res;
}

void Parameter::get_display_of_modulation_depth(char *txt, float modulationDepth )
{
   int detailedMode = false;
   if( storage )
      detailedMode = Surge::Storage::getUserDefaultValue(storage, "highPrecisionReadouts", 0);
   sprintf(txt, "%.*f", (detailedMode ? 6 : 2), modulationDepth);
}

void Parameter::get_display_alt(char* txt, bool external, float ef)
{
   
   txt[0] = 0;
   switch( ctrltype )
   {
   case ct_freq_hpf:
   case ct_freq_audible:
   case ct_freq_vocoder_low:
   case ct_freq_vocoder_high:
   {
      float f = val.f;
      int i_value = (int)( f + 0.5 ) + 69; // that 1/2th centers us
      if( i_value < 0 ) i_value = 0; 

      int oct_offset = 1;
      if (storage)
         oct_offset = Surge::Storage::getUserDefaultValue(storage, "middleC", 1);
      char notename[16];
      sprintf(txt, "%s", get_notename(notename, i_value, oct_offset));

      break;
   }
   case ct_flangerpitch:
   {
      float f = val.f;
      int i_value = (int)( f );
      if( i_value < 0 ) i_value = 0; 

      int oct_offset = 1;
      if (storage)
         oct_offset = Surge::Storage::getUserDefaultValue(storage, "middleC", 1);
      char notename[16];
      sprintf(txt, "%s", get_notename(notename, i_value, oct_offset));

      break;
   }
   case ct_countedset_percent:
      if (user_data != nullptr)
      {
         // We check when set so the reinterpret cast is safe and fast
         float f = val.f;
         CountedSetUserData* cs = reinterpret_cast<CountedSetUserData*>(user_data);
         auto count = cs->getCountedSetSize();
         auto tl = count * f;
         sprintf(txt, "%.2f / %d", tl, count);
      }

      break;
   }
}

void Parameter::get_display(char* txt, bool external, float ef)
{
   if (ctrltype == ct_none)
   {
      sprintf(txt, "-");
      return;
   }

   int i;
   float f;
   bool b;

   int detailedMode = 0;

   if (storage)
      detailedMode = Surge::Storage::getUserDefaultValue(storage, "highPrecisionReadouts", 0);

   switch (valtype)
   {
   case vt_float:
      if (external)
         f = ef * (val_max.f - val_min.f) + val_min.f;
      else
         f = val.f;

      switch (ctrltype)
      {
      case ct_portatime:
      case ct_envtime:
      case ct_envtime_lfodecay:
      case ct_reverbtime:
      case ct_reverbpredelaytime:
      case ct_chorusmodtime:
      case ct_delaymodtime:
         if ((ctrltype == ct_envtime_lfodecay) && (f == val_max.f))
         {
            sprintf(txt, "Forever");
         }
         else
         {
            if (temposync)
            {
               std::string res = tempoSyncNotationValue(f);
               sprintf(txt, "%s", res.c_str() );
            }
            else if (f == val_min.f)
            {
               sprintf(txt, "%.*f s", (detailedMode ? 6 : 3), 0.f);
            }
            else
            {
               sprintf(txt, "%.*f s", (detailedMode ? 6 : 3), powf(2.0f, val.f));
            }
         }
         break;
      case ct_lforate:
         if (temposync)
         {
            std::string res = tempoSyncNotationValue(-f);
            sprintf(txt, "%s", res.c_str() );
         }
         else
            sprintf(txt, "%.*f Hz", (detailedMode ? 6 : 3), powf(2.0f, f));
         break;
      case ct_amplitude:
         if (f == 0)
            sprintf(txt, "-inf dB");
         else
            sprintf(txt, "%.*f dB", (detailedMode ? 6 : 2), amp_to_db(f));
         break;
      case ct_decibel:
      case ct_decibel_attenuation:
      case ct_decibel_attenuation_large:
      case ct_decibel_fmdepth:
      case ct_decibel_narrow:
      case ct_decibel_extra_narrow:
         sprintf(txt, "%.*f dB", (detailedMode ? 6 : 2), f);
         break;
      case ct_decibel_extendable:
      case ct_decibel_narrow_extendable:
         sprintf(txt, "%.*f dB", (detailedMode ? 6 : 2), get_extended(f));
         break;
      case ct_bandwidth:
         sprintf(txt, "%.*f octaves", (detailedMode ? 6 : 2), f);
         break;
      case ct_freq_shift:
         sprintf(txt, "%.*f Hz", (detailedMode ? 6 : 2), get_extended(f));
         break;
      case ct_percent:
      case ct_percent_bidirectional:
      case ct_countedset_percent:
         sprintf(txt, "%.*f %c", (detailedMode ? 6 : 2), f * 100.f, '%');
         break;
      case ct_oscspread:
         if (absolute)
            sprintf(txt, "%.*f Hz", (detailedMode ? 6 : 2), 16.f * get_extended(f));
         else
            sprintf(txt, "%.*f cents", (detailedMode ? 6 : 2), get_extended(f) * 100.f);
         break;
      case ct_detuning:
         sprintf(txt, "%.*f cents", (detailedMode ? 6 : 2), f * 100.f);
         break;
      case ct_stereowidth:
         sprintf(txt, "%.*fº", (detailedMode ? 6 : 2), f);
         break;
      case ct_freq_hpf:
      case ct_freq_audible:
      case ct_freq_vocoder_low:
      case ct_freq_vocoder_high:
         sprintf(txt, "%.*f Hz", (detailedMode ? 6 : 3), 440.f * powf(2.0f, f / 12.f));
         break;
      case ct_pitch:
      case ct_syncpitch:
      case ct_freq_mod:
         sprintf(txt, "%.*f %s", (detailedMode ? 6 : 2), f,
                 (storage && !storage->isStandardTuning ? "keys" : "semitones"));
         break;
      case ct_pitch_semi7bp:
         sprintf(txt, "%.*f %s", (detailedMode ? 6 : 2), get_extended(f),
                 (storage && !storage->isStandardTuning ? "keys" : "semitones"));
          break;
      case ct_pitch_semi7bp_absolutable:
         if(absolute)
            sprintf(txt, "%.*f Hz", (detailedMode ? 6 : 2), 10.f * get_extended(f));
         else
            sprintf(txt, "%.*f %s", (detailedMode ? 6 : 2), get_extended(f),
                    (storage && !storage->isStandardTuning ? "keys" : "semitones"));
         break;
      case ct_fmratio:
         sprintf(txt, "C : %.*f", (detailedMode ? 6 : 2), f);
         break;
      case ct_flangerpitch:
         sprintf(txt, "%.*f", (detailedMode ? 6 : 2), f);
         break;
      case ct_osc_feedback:
         sprintf(txt, "%.*f %c", (detailedMode ? 6 : 2), get_extended(f) * 100.f, '%');
         break;
      default:
         sprintf(txt, "%.*f", (detailedMode ? 6 : 2), f);
         break;
      }
      break;
   case vt_int:
      if (external)
         i = (int)((1 / 0.99) * (ef - 0.005) * (float)(val_max.i - val_min.i) + 0.5) + val_min.i;
      else
         i = val.i;

      switch (ctrltype)
      {
      case ct_midikey_or_channel:
      {
         auto sm = storage->getPatch().scenemode.val.i;

         if (sm == sm_chsplit)
         {
            sprintf(txt, "Channel %d", (val.i / 8) + 1);
            break;
         }
      }
      case ct_midikey:
      {
         int oct_offset = 1;
         if (storage)
            oct_offset = Surge::Storage::getUserDefaultValue(storage, "middleC", 1);
         char notename[16];
         sprintf(txt, "%s", get_notename(notename, val.i, oct_offset));
         break;
      }
      case ct_osctype:
         sprintf(txt, "%s", osctype_abberations[limit_range(i, 0, (int)num_osctypes - 1)]);
         break;
      case ct_wt2window:
         sprintf(txt, "%s", window_abberations[limit_range(i, 0, 8)]);
         break;
      case ct_osccount:
      case ct_osccountWT:
         sprintf(txt, "%d voice%s", i, (i > 1 ? "s" : ""));
         break;
      case ct_fxtype:
         sprintf(txt, "%s", fxtype_abberations[limit_range(i, 0, (int)num_fxtypes - 1)]);
         break;
      case ct_reverbshape:
         sprintf(txt, "Type %d", i + 1);
         break;
      case ct_fxbypass:
         sprintf(txt, "%s", fxbypass_abberations[limit_range(i, 0, (int)n_fx_bypass - 1)]);
         break;
      case ct_filtertype:
         sprintf(txt, "%s", fut_abberations[limit_range(i, 0, (int)n_fu_type - 1)]);
         break;
      case ct_wstype:
         sprintf(txt, "%s", wst_abberations[limit_range(i, 0, (int)n_ws_type - 1)]);
         break;
      case ct_envmode:
         sprintf(txt, "%s", em_abberations[limit_range(i, 0, (int)n_em_type - 1)]);
         break;
      case ct_fbconfig:
         sprintf(txt, "%s", fbc_abberations[limit_range(i, 0, (int)n_fb_configuration - 1)]);
         break;
      case ct_fmconfig:
         sprintf(txt, "%s", fmc_abberations[limit_range(i, 0, (int)n_fm_configuration - 1)]);
         break;
      case ct_lfoshape:
         sprintf(txt, "%s", ls_abberations[limit_range(i, 0, (int)n_lfoshapes - 1)]);
         break;
      case ct_scenemode:
         sprintf(txt, "%s", scenemode_abberations[limit_range(i, 0, (int)n_scenemodes - 1)]);
         break;
      case ct_polymode:
         sprintf(txt, "%s", polymode_abberations[limit_range(i, 0, (int)n_polymodes - 1)]);
         break;
      case ct_lfotrigmode:
         sprintf(txt, "%s", lfomode_abberations[limit_range(i, 0, (int)n_lfomodes - 1)]);
         break;
      case ct_character:
         sprintf(txt, "%s", character_abberations[limit_range(i, 0, (int)n_charactermodes - 1)]);
         break;
      case ct_fmratio_int:
         sprintf(txt, "C : %d", i);
         break;
      case ct_sineoscmode:
         switch (i)
         {
         case 0:
            sprintf(txt, "Wave %d (TX 1)", i + 1);
            break;
         case 4:
            sprintf(txt, "Wave %d (TX 2)", i + 1);
            break;
         case 6:
            sprintf(txt, "Wave %d (TX 5)", i + 1);
            break;
         case 9:
            sprintf(txt, "Wave %d (TX 3)", i + 1);
            break;
         case 10:
            sprintf(txt, "Wave %d (TX 4)", i + 1);
            break;
         case 11:
            sprintf(txt, "Wave %d (TX 6)", i + 1);
            break;
         case 12:
            sprintf(txt, "Wave %d (TX 7)", i + 1);
            break;
         case 13:
            sprintf(txt, "Wave %d (TX 8)", i + 1);
            break;
         default:
            sprintf(txt, "Wave %d", i + 1);
         }
         break;
      case ct_sinefmlegacy:
         if (i == 0)
             sprintf( txt, "Legacy (before v1.6.2)");
         else
             sprintf( txt, "Consistent with FM2/3" );
         break;
      case ct_vocoder_bandcount:
         sprintf(txt, "%d bands", i);
         break;
      case ct_distortion_waveshape:
         // FIXME - do better than this of course
         switch( i + 1 )
         {
         case wst_tanh:
            sprintf(txt,"Soft");
            break;
         case wst_hard:
            sprintf(txt,"Hard");
            break;
         case wst_asym:
            sprintf(txt,"Asymmetric");
            break;
         case wst_sinus:
            sprintf(txt,"Sine");
            break;
         case wst_digi:
            sprintf(txt,"Digital");
            break;
         default:
         case wst_none:
            sprintf(txt,"None");
            break;
         }
         break;
      case ct_oscroute:
         switch (i)
         {
         case 0:
            sprintf(txt, "Filter 1");
            break;
         case 1:
            sprintf(txt, "Both");
            break;
         case 2:
            sprintf(txt, "Filter 2");
            break;
         }
         break;
      case ct_flangermode:
      {
         int mode = i;

         std::string types;
         switch( mode )
         {
         case 0:
            types = "Classic";
            break;
         case 1:
            types = "Classic Tuned";
            break;
         case 2:
            types = "Doppler";
            break;
         case 3:
            types = "Doppler Tuned";
            break;
         case 4:
            types = "Arpeggio Tuned Mixed";
            break;
         case 5:
            types = "Arpeggio Tuned Bare";
            break;
         }
         sprintf( txt, "%s", types.c_str() );
      }
      break;
      case ct_flangerwave:
      {
         switch( i )
         {
         case 0:
            sprintf( txt, "sin" );
            break;
         case 1:
            sprintf( txt, "tri" );
            break;
         case 2:
            sprintf( txt, "saw" );
            break;
         case 3:
            sprintf( txt, "s&h" );
            break;
         }
      }
      break;
      case ct_flangerchord:
      {
         int mode = i;

         std::string types;
         switch( mode )
         {
         case 0:
            types = "Unison";
            break;
         case 1:
            types = "Octaves";
            break;
         case 2:
            types = "Minor 2nds";
            break;
         case 3:
            types = "Major 2nds";
            break;
         case 4:
            types = "Diminished";
            break;
         case 5:
            types = "Augmented";
            break;
         case 6:
            types = "4ths";
            break;
         case 7:
            types = "Tritones";
            break;
         case 8:
            types = "5ths";
            break;
         case 9:
            types = "Major";
            break;
         case 10:
            types = "Minor";
            break;
         case 11:
            types = "Dominant";
            break;
         case 12:
            types = "Major 7th";
            break;
         }
         sprintf( txt, "%s", types.c_str() );
      }
         break;
      default:
         sprintf(txt, "%i", i);
         break;
      };
      break;
   case vt_bool:
      if (external)
         b = ef > 0.5f;
      else
         b = val.b;
      if (b)
         sprintf(txt, "On");
      else
         sprintf(txt, "Off");
      break;
   };
}

float Parameter::get_value_f01()
{
   if (ctrltype == ct_none)
      return 0;
   switch (valtype)
   {
   case vt_float:
      return (val.f - val_min.f) / (val_max.f - val_min.f);
      break;
   case vt_int:
      return 0.005 + 0.99 * ((float)(val.i - val_min.i)) / ((float)(val_max.i - val_min.i));
      break;
   case vt_bool:
      return val.b ? 1.f : 0.f;
      break;
   };
   return 0;
}

float Parameter::normalized_to_value(float value)
{
   switch (valtype)
   {
   case vt_float:
      return value * (val_max.f - val_min.f) + val_min.f;
      break;
   case vt_int:
      return value * ((float)val_max.i - (float)val_min.i) + (float)val_min.i;
      break;
   case vt_bool:
      return (value > 0.5f) ? 1.f : 0.f;
      break;
   };
   return 0;
}

float Parameter::value_to_normalized(float value)
{
   switch (valtype)
   {
   case vt_float:
      return (value - val_min.f) / (val_max.f - val_min.f);
      break;
   case vt_int:
      return ((float)value - (float)val_min.i) / ((float)val_max.i - (float)val_min.i);
      break;
   case vt_bool:
      return val.b ? 1.f : 0.f;
      break;
   };
   return 0;
}

const wchar_t* Parameter::getUnit() const
{
   return L"";
}

float Parameter::get_default_value_f01()
{
   if (ctrltype == ct_none)
      return 0;
   switch (valtype)
   {
   case vt_float:
      return (val_default.f - val_min.f) / (val_max.f - val_min.f);
      break;
   case vt_int:
      return 0.005 + 0.99 * ((float)(val_default.i - val_min.i)) / ((float)(val_max.i - val_min.i));
      // return ((float)(val_default.i-val_min.i))/((float)(val_max.i - val_min.i));
      break;
   case vt_bool:
      return val_default.b ? 1.f : 0.f;
      break;
   };
   return 0;
}

void Parameter::set_value_f01(float v, bool force_integer)
{
   switch (valtype)
   {
   case vt_float:
      val.f = v * (val_max.f - val_min.f) + val_min.f;
      break;
   case vt_int:
      val.i = (int)((1 / 0.99) * (v - 0.005) * (float)(val_max.i - val_min.i) + 0.5) + val_min.i;
      break;
   case vt_bool:
      val.b = (v > 0.5f);
      break;
   }
   bound_value(force_integer);
}

float Parameter::get_modulation_f01(float mod)
{
   if (ctrltype == ct_none)
      return 0;
   if (valtype != vt_float)
      return 0;
   //	float v = ((val.f+mod)-val_min.f)/(val_max.f - val_min.f);
   float v = (mod) / (val_max.f - val_min.f);
   // return limit_range(v,val_min.f,val_max.f);
   // return limit_range(v,0.0f,1.0f);
   return limit_range(v, -1.0f, 1.0f);
}

float Parameter::set_modulation_f01(float v)
{
   if (ctrltype == ct_none)
      return 0;
   if (valtype != vt_float)
      return 0;

   // float mod = v*(val_max.f - val_min.f) + val_min.f - val.f;
   float mod = v * (val_max.f - val_min.f);
   return mod;
}

//
pdata Parameter::morph(Parameter* b, float x)
{
   pdata rval;
   if ((valtype == vt_float) && (b->valtype == vt_float) && (ctrltype == b->ctrltype))
   {
      rval.f = (1 - x) * val.f + x * b->val.f;
   }
   else
   {
      if (x > 0.5)
         rval.i = b->val.i;
      else
         rval.i = this->val.i;
   }
   return rval;
}

// uses "this" as parameter a
/*void parameter::morph(parameter *b, float x)
{
        if((valtype == vt_float)&&(b->valtype == vt_float)&&(ctrltype == b->ctrltype))
        {
                val.f = (1-x)*val.f + x*b->val.f;
        }
        else
        {
                                        if (x>0.5)
                                                memcpy(this,b,sizeof(parameter));
        }
}*/

// uses two other parameters
void Parameter::morph(Parameter* a, Parameter* b, float x)
{
   if ((a->valtype == vt_float) && (b->valtype == vt_float) && (a->ctrltype == b->ctrltype))
   {
      memcpy((void*)this, (void*)a, sizeof(Parameter));
      val.f = (1 - x) * a->val.f + x * b->val.f;
   }
   else
   {
      if (x > 0.5)
         memcpy((void*)this, (void*)b, sizeof(Parameter));
      else
         memcpy((void*)this, (void*)a, sizeof(Parameter));
   }
}

bool Parameter::can_setvalue_from_string()
{
   switch( ctrltype )
   {
   case ct_percent:
   case ct_percent_bidirectional:
   case ct_pitch_semi7bp:
   case ct_pitch_semi7bp_absolutable:
   case ct_pitch:
   case ct_fmratio:
   case ct_syncpitch:
   case ct_amplitude:
   case ct_decibel:
   case ct_decibel_narrow:
   case ct_decibel_narrow_extendable:
   case ct_decibel_extra_narrow:
   case ct_decibel_attenuation:
   case ct_decibel_attenuation_large:
   case ct_decibel_fmdepth:
   case ct_decibel_extendable:
   case ct_freq_audible:
   case ct_freq_shift:
   case ct_freq_hpf:
   case ct_freq_vocoder_low:
   case ct_freq_vocoder_high:
   case ct_bandwidth:
   case ct_envtime:
   case ct_envtime_lfodecay:
   case ct_delaymodtime:
   case ct_reverbtime:
   case ct_reverbpredelaytime:
   case ct_portatime:
   case ct_lforate:
   case ct_detuning:
   case ct_oscspread:
   case ct_countedset_percent:
   case ct_flangerpitch:
   case ct_flangervoices:
   case ct_osc_feedback:
   case ct_chorusmodtime:
   case ct_pbdepth:
   case ct_polylimit:
   case ct_midikey:
   case ct_midikey_or_channel:
     {
      return true;
      break; }
   }
   return false;
}

bool Parameter::set_value_from_string( std::string s )
{
   const char* c = s.c_str();

   if( valtype == vt_int )
   {
      int ni = val_min.i - 1;   // default out of range value to test against later
      
      try
      {
         ni = std::stoi(c);
      }
      catch (const std::invalid_argument &)
      {
         ni = val_min.i - 1;   // set value of ni out of range on invalid input
      }
      catch (const std::out_of_range &)
      {
         ni = val_min.i - 1;   // same for out of range input
      }

      switch (ctrltype)
      {
      case ct_midikey_or_channel:
      {
         auto sm = storage->getPatch().scenemode.val.i;

         if (sm == sm_chsplit)
         {
            const char* strip = &(c[0]);
            while (*strip != '\0' && !std::isdigit(*strip))
               ++strip;
            ni = (std::atof(strip) * 8) - 1;

            // breaks case after channel number input, but if we're in split mode we fall through to ct_midikey
            break;
         }
      }
      case ct_midikey:
      {
         if (ni == val_min.i - 1)   // if integer input failed, try note recognition
         {
            std::string::size_type n;
            std::string::size_type m;
            std::string notes[7] = {"c", "d", "e", "f", "g", "a", "b"};
            int pitches[7] = {0, 2, 4, 5, 7, 9, 11};
            int val = 0;
            int neg = 1;

            // convert string to lowercase
            std::for_each(s.begin(), s.end(),
                [](char &c) { c = ::tolower(static_cast<unsigned char>(c)); });

            // find the unmodified note
            for (int i = 0; i < 7; i++)
            {
               n = s.find(notes[i]);
               if (n != std::string::npos)
               {
                  val = pitches[i];
                  break;
               }
            }

            // check if the following character is sharp or flat, adjust val if so
            n++;
            if (( m = s.find("#", n, 1) ) != std::string::npos)
            {
               val += 1;
               n++;
            }
            else if ((m = s.find("b", n, 1)) != std::string::npos)
            {
               val -= 1;
               n++;
            }

            // if neither note modifiers are found, check for minus
            if ((m = s.find("-", n, 1)) != std::string::npos)
            {
               neg = -1;
               n++;
            }

            // finally, octave number
            s = s.substr(n, s.length() - n);   // trim the fat to the left of current char

            int oct;
            try
            {
               oct = std::stoi(s);
            }
            catch (std::invalid_argument const &)
            {
               oct = -10;   // throw things out of range on invalid input
            }

             // construct the integer note value
            int oct_offset = 1;
            if (storage)
               oct_offset = Surge::Storage::getUserDefaultValue(storage, "middleC", 1);

            ni = ((oct + oct_offset) * 12 * neg) + val;
         }

         break;
      }
      }

      if (ni >= val_min.i && ni <= val_max.i)
      {
         val.i = ni;
         return true;
      }

      return false;
   }

   auto nv = std::atof(c);

   switch (ctrltype)
   {
   case ct_percent:
   case ct_percent_bidirectional:
   case ct_countedset_percent:
   case ct_detuning:
   {
      if (nv < val_min.f * 100 || nv > val_max.f * 100) {
         return false; }

      val.f = nv / 100.0;

      break;
   }
   case ct_pitch_semi7bp:
   case ct_pitch_semi7bp_absolutable:
   case ct_pitch: {
      // factors 12 and 10 need to be consistent with get_extended and get_absolute returns for these ctypes
      // if those returns are changed at some point, the factors also need to be changed here
      int ext_mul = 1 + (11 * (int)extend_range);
      int abs_mul = 1 + (9 * (int)absolute);

      int factor = ext_mul * abs_mul;
      int limit = val_max.f * factor;

      if (nv < -limit || nv > limit) {
         return false; }

      val.f = nv / factor;

      break;
   }
   case ct_amplitude:
   {
      // typing in the maximum value for send levels (12 dB) didn't work
      // probably because of float precision (or lack thereof)
      // so special case them here
      // better solution welcome!
      if (nv == 12) {
         nv = limit_range((float)db_to_amp(nv), val_min.f, val_max.f); }
      else {
         nv = db_to_amp(nv); }
      
      if (nv < val_min.f || nv > val_max.f) {
         return false; }

      val.f = nv;

      break;
   }
   case ct_fmratio:
   {
      // In this case we have to set nv differently
      const char *strip = &(c[0]);
      while( *strip != '\0' && ! std::isdigit( *strip ) ) ++strip;
      nv = std::atof( strip );
   }
   case ct_decibel:
   case ct_decibel_narrow:
   case ct_decibel_extra_narrow:
   case ct_decibel_attenuation:
   case ct_decibel_attenuation_large:
   case ct_decibel_fmdepth:
   case ct_syncpitch:
   case ct_bandwidth:
   case ct_flangerpitch:
   case ct_flangervoices:
   {
      if (nv < val_min.f || nv > val_max.f) {
         return false; }

      val.f = nv;

      break;
   }
   case ct_decibel_narrow_extendable:
   case ct_decibel_extendable:
   {
      int factor = 1 + ((ctrltype == ct_decibel_narrow_extendable ? 4 : 2) * (int)extend_range); // 5 for dB narrow extendable, 3 for dB extendable

      int limit = val_max.f * factor;

      if (nv < -limit || nv > limit) {
         return false; }

      val.f = nv / factor;

      break;
   }
   case ct_freq_hpf:
   case ct_freq_audible:
   case ct_freq_vocoder_low:
   case ct_freq_vocoder_high:
   {  
      float oldval = val.f;

      // these are 440 * 2 ^ ( f / 12 ) = c
      // so log2( c / 440 ) * 12 = f;
      val.f = log2f(nv / 440.0) * 12.0f;

      if (val.f < val_min.f || val.f > val_max.f)
      {
         val.f = oldval;
         return false;
      }

      break;
   }
   case ct_freq_shift:
   {
      int factor = 1 + ((int)extend_range * 99);
      int limit = 10 + (990 * (int)extend_range); // x10 when normal, x1000 when extended
      
      if (nv < -limit || nv > limit) {
          return false; }
      else {
         val.f = nv / factor; }

      break;
   }
   case ct_envtime:
   case ct_envtime_lfodecay:
   case ct_delaymodtime:
   case ct_reverbtime:
   case ct_reverbpredelaytime:
   case ct_portatime:
   case ct_lforate:
   case ct_chorusmodtime:
   {
      float oldval = val.f;
      
      val.f = log2f(nv);

      if (val.f < val_min.f || val.f > val_max.f)
      {
         val.f = oldval;
         return false;
      }

      break;
   }
   case ct_oscspread: {
      int ext_mul = 1 + (11 * (int)extend_range); // x12 when extended
      int abs_mul = 100 - (84 * (int)absolute);   // x100 for cents, x16 for Hz (absolute)

      int range = val_max.f * abs_mul * ext_mul;

      if (nv < 0 || nv > range) {
         return false; }

      val.f = nv / range;

      break;
   }
   case ct_osc_feedback:
   {
      if (extend_range)
      {
         if (nv < -400 || nv > 400) {
            return false; }

         val.f = (nv * 0.00125) + 0.5f;
      }
      else
      {
         if (nv < 0 || nv > 100) {
            return false; }

         val.f = nv / 100.f;
      }

      break;
   }

   default:
      return false;
   }
   return true;
}

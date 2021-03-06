//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"
#include "SurgeStorage.h"
#include "CDIBitmap.h"
#include "DspUtilities.h"
#include "SkinSupport.h"
#include "SurgeBitmaps.h"
#include "CScalableBitmap.h"


class CLFOGui : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponnt
{
public:
   const static int margin = 2;
   const static int margin2 = 7;
   const static int lpsize = 50;
   const static int scale = 18;
   const static int shadowoffset = 1;
   const static int skugga = 0xff5d5d5d;
   const static int splitpoint = lpsize + 20;
    
   void drawtri(VSTGUI::CRect r, VSTGUI::CDrawContext* context, int orientation);

   CLFOGui(const VSTGUI::CRect& size,
           bool trigmaskedit,
           VSTGUI::IControlListener* listener = 0,
           long tag = 0,
           LFOStorage* lfodata = 0,
           SurgeStorage* storage = 0,
           StepSequencerStorage* ss = 0,
           std::shared_ptr<SurgeBitmaps> ibms = nullptr)
      : VSTGUI::CControl(size, listener, tag, 0),
        bitmapStore( ibms )
   {
      this->lfodata = lfodata;
      this->storage = storage;
      this->ss = ss;
      edit_trigmask = trigmaskedit;
      controlstate = 0;

      for( int i=0; i<16; ++i )
         draggedIntoTrigTray[i] = false;
   }

   void resetColorTable()
   {
      auto c = skin->getColor( "lfo.waveform.fill", VSTGUI::CColor( 0xFF, 0x90, 0x00 ) );
      auto d = skin->getColor( "lfo.waveform.wave", VSTGUI::CColor( 0x00, 0x00, 0x00 ) );
      
   }
   // virtual void mouse (CDrawContext *pContext, VSTGUI::CPoint &where, long buttons = -1);
   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual bool onWheel(const VSTGUI::CPoint& where, const float& distance, const VSTGUI::CButtonState& buttons) override;

   virtual void setSkin( Surge::UI::Skin::ptr_t s ) override {
      SkinConsumingComponnt::setSkin(s);
      resetColorTable();
   }
   
   virtual ~CLFOGui()
   {
   }
   virtual void draw(VSTGUI::CDrawContext* dc) override;
   void drawStepSeq(VSTGUI::CDrawContext *dc, VSTGUI::CRect &maindisp, VSTGUI::CRect &leftpanel);
   
   void invalidateIfIdIsInRange(int id);
   void invalidateIfAnythingIsTemposynced();

   void setTimeSignature(int n, int d ) {
      tsNum = n;
      tsDen = d;
   }

protected:
   LFOStorage* lfodata;
   StepSequencerStorage* ss;
   SurgeStorage* storage;
   std::shared_ptr<SurgeBitmaps> bitmapStore;
   int tsNum = 4, tsDen = 4;
   
   
   VSTGUI::CRect shaperect[n_lfoshapes];
   VSTGUI::CRect steprect[n_stepseqsteps];
   VSTGUI::CRect gaterect[n_stepseqsteps];
   VSTGUI::CRect rect_ls, rect_le, rect_shapes, rect_steps, rect_steps_retrig;
   VSTGUI::CRect ss_shift_left, ss_shift_right;
   bool edit_trigmask;
   int controlstate;
   int selectedSSrow = -1;

   int draggedStep = -1;
   int keyModMult = 0;

   bool draggedIntoTrigTray[16];
   int mouseDownTrigTray = -1;
   
   CLASS_METHODS(CLFOGui, VSTGUI::CControl)
};

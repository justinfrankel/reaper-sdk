/*
**

  REAPER example_m3u plug-in 
  Copyright (C) 2005-2008 Cockos Incorporated

  provides m3u as project import (not very useful but a good demonstration)


  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.


  */


#include <windows.h>
#include <stdio.h>
#include <math.h>


#include "../reaper_plugin.h"


REAPER_PLUGIN_HINSTANCE g_hInst; // used for dialogs, if any

// these are used to resolve/make relative pathnames 
void (*resolve_fn)(const char *in, char *out, int outlen);
void (*relative_fn)(const char *in, char *out, int outlen);
PCM_source *(*PCM_Source_CreateFromFile)(const char *filename);



bool WantProjectFile(const char *fn)
{
  return strlen(fn)>4 && !stricmp(fn+strlen(fn)-4,".m3u");
}

  // this is used for UI only
const char *EnumFileExtensions(int i, const char **descptr) // call increasing i until returns a string, if descptr's output is NULL, use last description
{
  if (i == 0)
  {
    if (descptr) *descptr = "M3U playlists";
    return "M3U";
  }
  if (descptr) *descptr=NULL;
  return NULL;
}

int LoadProject(const char *fn, ProjectStateContext *ctx) // return 0=ok,
{
  // no need to check extension here, it's done for us
  FILE *fp=fopen(fn,"rt");
  if (!fp) return -1; // error!
  
  ctx->AddLine("<REAPER_PROJECT 0.1");
  // set any project settings here (this example doesnt need to)


  // add a track
  ctx->AddLine("<TRACK");
    ctx->AddLine("NAME \"m3u playlist\"");


  double lastfadeouttime=0.0;
  double curpos=0.0;
  // add an item per playlist item
  for (;;)
  {
    char buf[1024];
    buf[0]=0;
    fgets(buf,sizeof(buf),fp);
    if (!buf[0]) break;

    if (buf[strlen(buf)-1]=='\n') buf[strlen(buf)-1]=0;

    if (!buf[0]) continue;

    if (buf[0]!='#' && !strstr(buf,"://"))
    {
      double thisl=30.0;
      char ofn[2048];
      resolve_fn(buf,ofn,sizeof(ofn));
      PCM_source *src=PCM_Source_CreateFromFile(ofn);
      if (src) 
      {
        thisl=src->GetLength();
        delete src;
      }
      else
      {
        // todo: use extended info from the m3u
      }
      char *fnptr=ofn;
      // todo: extended m3u info
      while (*fnptr) fnptr++;
      while (fnptr>=ofn && *fnptr !='\\' && *fnptr != '/') fnptr--;
      fnptr++;

      double thisfadeouttime=3.0; // default to 3s xfades
      if (thisfadeouttime>thisl)
      {
        thisfadeouttime=0.01;
        if (lastfadeouttime+thisfadeouttime>thisl)
          lastfadeouttime=thisl-thisfadeouttime;
      }

      ctx->AddLine("<ITEM");
        ctx->AddLine("POSITION %.8f",curpos);
        ctx->AddLine("LENGTH %.8f",thisl);
        ctx->AddLine("FADEIN 1 0.01 %f",lastfadeouttime);
        ctx->AddLine("FADEOUT 1 0.01 %f",thisfadeouttime);
        ctx->AddLine("LOOP 0"); // turn off looping
        ctx->AddLine("NAME \"%s\"",fnptr); 
        ctx->AddLine("SRCFN \"%s\"",ofn);
      ctx->AddLine(">");
      
      lastfadeouttime=thisfadeouttime;
      curpos+=thisl-thisfadeouttime;
    }
  }
  ctx->AddLine(">"); // <TRACK
  
  ctx->AddLine(">"); // <REAPER_PROJECT

  fclose(fp);
  return 0;
}


project_import_register_t myRegStruct={WantProjectFile,EnumFileExtensions,LoadProject};


extern "C"
{

REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t *rec)
{
  g_hInst=hInstance;
  if (rec)
  {
    if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc)
      return 0;

    *((void **)&resolve_fn) = rec->GetFunc("resolve_fn");   
    *((void **)&relative_fn) = rec->GetFunc("relative_fn");   
    *((void **)&PCM_Source_CreateFromFile) = rec->GetFunc("PCM_Source_CreateFromFile");

    if (!resolve_fn || 
      !PCM_Source_CreateFromFile || 
      !rec->Register ||      
      !rec->Register("projectimport",&myRegStruct))
      return 0;

    // our plugin registered, return success

    return 1;
  }
  else
  {
    return 0;
  }
}

};






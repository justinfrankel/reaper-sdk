#ifndef _TAG_H_
#define _TAG_H_

#include "../WDL/ptrlist.h"
#include "../WDL/assocarray.h"
#include "../WDL/wdlstring.h"
#include "../WDL/wdlcstring.h"
#include "../WDL/wdlutf8.h"
#include "../WDL/fileread.h"

#include "metadata.h"


char *tag_strndup(const char *src, int len);


#define TAG_HEXDUMP_PREFIX "[Binary data] "
#define TAG_HEXDUMP_PREFIX_LEN 14

static const char *s_genres[] = {
// !WANT_LOCALIZE_STRINGS_BEGIN:tag_genre
  "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk",
  "Grunge", "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other",
  "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial",
  "Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack",
  "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion",
  "Trance", "Classical", "Instrumental", "Acid", "House", "Game",
  "Sound Clip", "Gospel", "Noise", "AlternRock", "Bass", "Soul", "Punk",
  "Space", "Meditative", "Instrumental Pop", "Instrumental Rock",
  "Ethnic", "Gothic", "Darkwave", "Techno-Industrial", "Electronic",
  "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult",
  "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle",
  "Native American", "Cabaret", "New Wave", "Psychadelic", "Rave",
  "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz",
  "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock",
  // Winamp extensions
  "Folk", "Folk-Rock", "National Folk", "Swing", "Fast Fusion",
  "Bebob", "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde",
  "Gothic Rock", "Progressive Rock", "Psychedelic Rock",
  "Symphonic Rock", "Slow Rock", "Big Band", "Chorus",
  "Easy Listening", "Acoustic", "Humour", "Speech", "Chanson",
  "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass",
  "Primus", "Porn Groove", "Satire", "Slow Jam", "Club", "Tango",
  "Samba", "Folklore", "Ballad", "Power Ballad", "Rhytmic Soul",
  "Freestyle", "Duet", "Punk Rock", "Drum Solo", "Acapella",
  "Euro-House", "Dance Hall",
  // Lame extensions
  "Goa", "Drum & Bass", "Club-House", "Hardcore", "Terror", "Indie",
  "BritPop", "Anotherpunk", "Polsk Punk", "Beat", "Christian Gangsta",
  "Heavy Metal", "Black Metal", "Crossover", "Contemporary Christian",
  "Christian Rock", "Merengue", "Salsa", "Trash Metal", "Anime",
  "JPop", "SynthPop"
// !WANT_LOCALIZE_STRINGS_END
};

const char *GetGenre(const char *val)
{
  int g = atoi(val);
  if (!g && val[0]=='(') g=atoi(val+1); // support things like "(8)Jazz"
  if (!g && strcmp(val, "0") && strncmp(val, "(0", 2)) g = -1;
  if (g >= 0 && g < sizeof(s_genres)/sizeof(s_genres[0]))
  {
#ifdef _REAPER_LOCALIZE_H_
    return __localizeFunc(s_genres[g], "tag_genre", LOCALIZE_FLAG_NOCACHE);
#else
    return s_genres[g];
#endif
  }
  return val;
}


static const char *ID3_ETCO_TYPE[]=
{
  "padding",
  "end of initial silence",
  "intro start",
  "main part start",
  "outro start",
  "outro end",
  "verse start",
  "refrain start",
  "interlude start",
  "theme start",
  "variation start",
  "key change",
  "time change",
  "momentary unwanted noise",
  "sustained noise",
  "sustained noise end",
  "intro end",
  "main part end",
  "verse end",
  "refrain end",
  "theme end",
  "profanity",
  "profanity end"
};
int GetETCOType(const char *desc, int len)
{
  for (int i=0; i < sizeof(ID3_ETCO_TYPE)/sizeof(ID3_ETCO_TYPE[0]); ++i)
  {
    if (!strnicmp(desc, ID3_ETCO_TYPE[i], len)) return i;
  }
  return -1;
}

#define _GetInt32LE(p) \
(((p)[0])|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24))

bool ParseAPEv2(unsigned char *buf, int buflen, int tagcnt,
  WDL_StringKeyedArray<char*> *metadata)
{
  bool has_tag=false;
  WDL_FastString keystr;
  while (buflen > 0 && tagcnt > 0)
  {
    int vallen=_GetInt32LE(buf);
    int flags=_GetInt32LE(buf+4);
    const char *key=(const char*)buf+8;
    const char *p=key;
    while (*p) ++p;
    const char *val=++p;
    int taglen=4+4+(val-key)+vallen;
    if ((flags>>1)&3) val=TAG_HEXDUMP_PREFIX;
    has_tag=true;
    keystr.Set("APE:");
    keystr.Append(key);
    metadata->Insert(keystr.Get(), tag_strndup(val, vallen));
    buf += taglen;
    buflen -= taglen;
    --tagcnt;
  }
  WDL_ASSERT(!buflen && !tagcnt);
  return has_tag;
}

int SynchSafeBytesToInt(bool issynchsafe, const char *bytes, int bytes_sz)
{
  if (!bytes || bytes_sz<=0 || bytes_sz>4) return -1;
  
  int i = -1;
  if (issynchsafe)
  {
    i=0;
    int b=0;
    if (bytes_sz>3) { if (bytes[b]>=0) i += ((int)bytes[b++])<<21; else return -1; }
    if (bytes_sz>2) { if (bytes[b]>=0) i += ((int)bytes[b++])<<14; else return -1; }
    if (bytes_sz>1) { if (bytes[b]>=0) i += ((int)bytes[b++])<<7; else return -1; }
    if (bytes[b]>=0) i += ((int)bytes[b++]); else return -1;
  }
  else
  {
    i=0;
    int b=0;
    if (bytes_sz>3) i += ((unsigned char)bytes[b++])<<24;
    if (bytes_sz>2) i += ((unsigned char)bytes[b++])<<16;
    if (bytes_sz>1) i += ((unsigned char)bytes[b++])<<8;
    i += ((unsigned char)bytes[b++]);
  }
  return i;
}

// returns the src length (+BOM, if any), or src_sz if src isn't NULL-terminated, or 0 on error
int ID3v2StringToUTF8(char enc, const char* src, int src_sz, WDL_FastString *dest)
{
  if (src_sz<=0) return 0;

  WDL_HeapBuf tmp;
  if (src[src_sz-1])
  {
    char *p=(char*)tmp.Resize(src_sz+1);
    memcpy(p, src, src_sz);
    p[src_sz++]=0;
    src=p;
  }

  if (enc==0) // iso-8859-1 (latin-1)
  {
    if (WDL_DetectUTF8(src)<0)
    {
      int i;
      char b[3];
      const unsigned char *rd=(const unsigned char *)src;
      for (i=0; i<src_sz && rd[i]; i++)
      {
        WDL_MakeUTFChar(b, rd[i], 3);
        dest->Append(b);
      }
      return i;
    }
    // else: ascii 7-bit or utf8 (marked as latin-1), fallthrough
  }
  else if (enc==1 || enc==2) // ucs2/utf16 with bom, or utf16be
  {
    // id3v2.3 uses ucs-2 which is identical to utf16 with bom (when decoding)

    const unsigned char *rd=(const unsigned char *)src;
    bool isLE = false;
    if (enc==1)
    {
      isLE = (rd[0]==0xFF && rd[1]==0xFE);
      if (!isLE && !(rd[0]==0xFE && rd[1]==0xFF)) return 0; // no bom
    }

    int i = (enc==1 ? 2 : 0);
    while (i<src_sz-1 && (rd[i] || rd[i+1]))
    {
      char buf[32];
      WDL_MakeUTFChar(buf,isLE ? (rd[i+1]<<8)+rd[i] : (rd[i]<<8)+rd[i+1],32);
      dest->Append(buf);
      i+=2; 
    }
  
    return i;
  }
  else if (enc==3) // utf8
  {
    // fallthrough
  }
  else // unknown encoding
  {
    return 0;
  }

  dest->Append(src, src_sz);
  return dest->GetLength();
}

#define IS_WHITESPACE(p) ((p) == ' ' || (p) == '\t' || (p) == '\r' || (p) == '\n')

bool AddID3v1Tag(const char *key, const char *val, WDL_StringKeyedArray<char*> *metadata)
{
  if (metadata->Exists(key)) return false;
  const char *start=val;
  while (*start && IS_WHITESPACE(*start)) ++start;
  const char *end=start+strlen(start)-1;
  while (end >= start && IS_WHITESPACE(*end)) --end;
  if (end < start) return false;
  metadata->Insert(key, tag_strndup(start, end-start+1));
  return true;
}

bool ParseID3v1(const char *buf, WDL_StringKeyedArray<char*> *metadata, bool fast)  // fast: only parse common tags (parsing en masse)
{
  bool has_tag=false;

  char tag[31];
  bool id3v11 = (!buf[125] && buf[126]);

  memcpy(tag, buf+3, 30);
  tag[30]=0;
  if (AddID3v1Tag("ID3:TIT2", tag, metadata)) has_tag=true;

  memcpy(tag, buf+33, 30);
  tag[30]=0;
  if (AddID3v1Tag("ID3:TPE1", tag, metadata)) has_tag=true;

  memcpy(tag, buf+63, 30);
  tag[30]=0;
  if (AddID3v1Tag("ID3:TALB", tag, metadata)) has_tag=true;

  memcpy(tag, buf+93, 4);
  tag[4]=0;
  if (atoi(tag) && !metadata->Exists("ID3:TYER"))
  {
    has_tag=true;
    metadata->Insert("ID3:TYER", strdup(tag));
  }

  memcpy(tag, buf+97, id3v11 ? 28 : 30);
  tag[id3v11 ? 28 : 30]=0;
  if (AddID3v1Tag("ID3:COMM", tag, metadata)) has_tag=true;

  unsigned char g=(unsigned char)buf[127];
  if (g < 255 && !metadata->Exists("ID3:TCON"))
  {
    snprintf(tag, sizeof(tag), "%d", g);
    const char *genre=GetGenre(tag);
    metadata->Insert("ID3:TCON", strdup(genre));
  }

  if (!fast && id3v11)
  {
    snprintf(tag, sizeof(tag), "%d", (unsigned char)buf[126]);
    if (atoi(tag) && !metadata->Exists("ID3:TRCK"))
    {
      has_tag=true;
      metadata->Insert("ID3:TRCK", strdup(tag));
    }
  }

  return has_tag;
}

// the input buffer can be mangled (when unsynchronisation is used)
bool ParseID3v2(char *buf, int tag_size,
  WDL_StringKeyedArray<char*> *metadata, bool fast, // fast: only parse common tags (parsing en masse)
  WDL_INT64 chunk_start_pos)
{
  bool has_tag=false;

  if (buf[5]&0x80) // unsynchronisation at tag level
  {
    unsigned char *p=(unsigned char *)buf+10;
    int i=0, j=0;
    while (i<tag_size)
    {
      p[j]=p[i];
      if (i < tag_size-1 && p[i]==0xFF && p[i+1]==0x00) i++;
      i++;
      j++;
    }
    tag_size=j;
  }

  int ext_hdr_sz = (buf[3]!=2 && (buf[5]&0x40)) ? SynchSafeBytesToInt(buf[3]==4, buf+10, 4) : 0;
  if (ext_hdr_sz<0) return false;

  WDL_HeapBuf hb;
  WDL_FastString frameid, frame_data, shortdesc, txt;
  int chapcnt=0;
  int id3_pos = 10+ext_hdr_sz;
  const int chunk_hdr_size = buf[3] == 2 ? 6 : 10;
  while (WDL_NORMALLY(id3_pos > 0) && id3_pos + chunk_hdr_size <= 10+tag_size)
  {
    if (!memcmp(buf+id3_pos, "\0\0\0", 3)) break; // we are into padding
    
    frameid.Set(buf+id3_pos, buf[3]==2 ? 3 : 4);
    id3_pos += (buf[3]==2 ? 3 : 4);
    
    int frame_sz = SynchSafeBytesToInt(buf[3]==4, buf+id3_pos, buf[3]==2 ? 3 : 4);
    if (frame_sz <= 0) break;
    id3_pos += (buf[3]==2 ? 3 : 4);
    if (id3_pos+frame_sz < 0 || id3_pos+frame_sz > 10+tag_size) break;
    
    if (frameid.GetLength() != (buf[3]==2 ? 3 : 4))
    {
      // invalid frame id, ignore it
      id3_pos += frame_sz;
      continue;
    }

    if (buf[3]!=2)
    {
      char fmt_flags=buf[id3_pos+1];
      id3_pos += 2; // skip flags
      
      if ((buf[3]==3 && (fmt_flags&0xDF)) || (buf[3]==4 && (fmt_flags&0xBF)))
      {
        // unsupported frame format, ignore it (extended, compressed, encrypted, unsynchronised, etc)
        id3_pos += frame_sz;
        continue;
      }
      else if ((buf[3]==3 && (fmt_flags&0x20)) || (buf[3]==4 && (fmt_flags&0x40)))
      {
        id3_pos++; // skip group byte, included in frame_sz though
        frame_sz--;
        if (WDL_NOT_NORMALLY(frame_sz <= 0)) continue; // maybe allow this for some tag types?
      }
    }
    
    int skip_bytes = 0;
    const char *p = buf+id3_pos;

    frame_data.Set("");      
    if (frameid.Get()[0]=='T' && strcmp(frameid.Get(), buf[3]==2 ? "TXX" : "TXXX"))
    {
      skip_bytes = 1;
      ID3v2StringToUTF8(p[0], p+skip_bytes, frame_sz-skip_bytes, &frame_data);
      if (!strcmp(frameid.Get(), buf[3] == 2 ? "TCO" : "TCON"))
      {
        const char *genre=GetGenre(frame_data.Get());
        if (genre != frame_data.Get()) frame_data.Set(genre);
      }
    }
    // text tags with encoding byte + short desc
    else if (!strcmp(frameid.Get(), buf[3]==2 ? "COM" : "COMM") || 
             (!fast && (!strcmp(frameid.Get(), buf[3]==2 ? "TXX" : "TXXX") || 
                        !strcmp(frameid.Get(), buf[3]==2 ? "WXX" : "WXXX") ||
                        !strcmp(frameid.Get(), buf[3]==2 ? "ULT" : "USLT"))))
    {
      skip_bytes = 1;
      
      const char *lang = NULL;
      if (!strncmp(frameid.Get(),"COM",3) || !strcmp(frameid.Get(),"ULT") || !strcmp(frameid.Get(),"USLT"))
      {
        lang=p+skip_bytes;
        skip_bytes+=3;
      }
      
      shortdesc.Set("");
      skip_bytes += ID3v2StringToUTF8(p[0], p+skip_bytes, frame_sz-skip_bytes, &shortdesc);
      if (skip_bytes<frame_sz) // null terminator found
      {
        skip_bytes += ((p[0]==1 || p[0]==2) ? 2 : 1);
        ID3v2StringToUTF8(p[0], p+skip_bytes, frame_sz-skip_bytes, &frame_data);

        if (lang && lang[0] >= 'a' && lang[0] <= 'z')
        {
          frameid.Append(":");
          frameid.Append(lang, 3);
        }
        if (shortdesc.GetLength())
        {
          frameid.Append(":");
          frameid.Append(shortdesc.Get());
        }
      }
    }
    // binary tags with short desc
    else if (!fast && (!strcmp(frameid.Get(),"PRIV") || !strcmp(frameid.Get(), buf[3]==2 ? "UFI" : "UFID")))
    {
      shortdesc.Set("");
      skip_bytes += ID3v2StringToUTF8(0, p+skip_bytes, frame_sz-skip_bytes, &shortdesc);
      if (skip_bytes<frame_sz)
      {
        skip_bytes += ((p[0]==1 || p[0]==2) ? 2 : 1);
        if (shortdesc.GetLength())
        {
          frameid.Append(":");
          frameid.Append(shortdesc.Get());
        }
      }
    }
    else if (!fast && frameid.Get()[0]=='W')
    {
      ID3v2StringToUTF8(0, p, frame_sz, &frame_data);
    }
    else if (!fast && !strcmp(frameid.Get(), buf[3]==2 ? "PIC" : "APIC"))
    {
      skip_bytes = 1;

      if (buf[3]==2)
      {
        frame_data.Set("ext:");
        frame_data.Append(p+skip_bytes, 3);
        skip_bytes+=3;
      }
      else
      {
        frame_data.Set("mime:");
        skip_bytes += ID3v2StringToUTF8(0, p+skip_bytes, frame_sz-skip_bytes, &frame_data);
        skip_bytes -= strlen("mime:"); // this might be a bug in ID3v2StringToUTF8()
      }
      if (skip_bytes<frame_sz)
      {
        if (buf[3]!=2) skip_bytes++;
        int pictype=p[skip_bytes++];
        WDL_INT64 dataoffs=0;

        shortdesc.Set("");
        skip_bytes += ID3v2StringToUTF8(p[0], p+skip_bytes, frame_sz-skip_bytes, &shortdesc);
        if (skip_bytes<frame_sz) // null terminator found
        {
          skip_bytes += ((p[0]==1 || p[0]==2) ? 2 : 1);
          dataoffs=chunk_start_pos+(WDL_INT64)(p-buf+skip_bytes);

          if (shortdesc.GetLength())
            metadata->Insert("ID3:APIC_DESC",strdup(shortdesc.Get()));
        }
        if (pictype >= 0 && pictype < 128)
        {
          char tmp[32];
          snprintf(tmp,sizeof(tmp),"%d",pictype);
          metadata->Insert("ID3:APIC_TYPE",strdup(tmp));
        }

        if (pictype >= 0 && pictype < 128 && dataoffs > 0 && skip_bytes < frame_sz)
        {
          if (frame_data.GetLength()) frame_data.Append(" ");
          frame_data.AppendFormatted(512, "offset:%" WDL_PRI_UINT64 " length:%d",
            dataoffs, frame_sz-skip_bytes);
        }
      }
    }
    else if (!fast && buf[3] != 2 && !strcmp(frameid.Get(), "CTOC"))
    {
      int pos=0;
      while (pos+2 < frame_sz && p[pos]) ++pos;
      if (pos+2 < frame_sz && p[pos+2])
      {
        frame_data.SetFormatted(512, "%d chapters", p[pos+2]);
      }
      if (!frame_data.Get()[0]) frame_data.Set("[table of contents data]");
    }
    else if (!fast && buf[3] != 2 && !strcmp(frameid.Get(), "CHAP"))
    {
      frameid.AppendFormatted(512, "%03d", ++chapcnt);

      int pos=0;
      while (pos < frame_sz && p[pos]) ++pos; // advance through CTOC child element
      ++pos;

      unsigned int startms=SynchSafeBytesToInt(false, p+pos, 4);
      pos += 4;
      unsigned int endms=SynchSafeBytesToInt(false, p+pos, 4);
      pos += 12;
      frame_data.SetFormatted(512, "%u:%u", startms, endms);

      if (pos+12 < frame_sz && !memcmp(p+pos, "TIT", 3))
      {
        pos += 4;
        int title_frame_sz=SynchSafeBytesToInt(buf[3]==4, p+pos, 4);
        pos += 6;
        char enc=p[pos++];

        txt.Set("");
        if (ID3v2StringToUTF8(enc, p+pos, title_frame_sz-1, &txt))
        {
          frame_data.AppendFormatted(512, ":%s", txt.Get());
        }
      }
    }
    else if (!fast && buf[3] != 2 && !strcmp(frameid.Get(), "ETCO"))
    {
      int pos=skip_bytes;
      if (((frame_sz-pos-1)%5) == 0)
      {
        // if (p[0] == 0x01) // timestamp is mpeg frames, todo
        if (p[0] == 0x02) // timestamp is ms
        {
          for (++pos; pos <= frame_sz-5; pos += 5)
          {
            unsigned char evt_type=p[pos];
            unsigned int timestamp=SynchSafeBytesToInt(false, p+pos+1, 4);

            const char *desc=
              evt_type < sizeof(ID3_ETCO_TYPE)/sizeof(ID3_ETCO_TYPE[0]) ?
              ID3_ETCO_TYPE[evt_type] : "unknown";
            frame_data.AppendFormatted(512, "%s%u:%s",
              frame_data.GetLength() ? "," : "", timestamp, desc);
          }
        }
      }
    }
    else
    {
      // hex dump
    }
    
    if (skip_bytes<frame_sz)
    {
      if (!fast && !frame_data.Get()[0])
      {
        frame_data.Set(TAG_HEXDUMP_PREFIX);
      }
      if (frame_data.Get()[0])
      {
        has_tag=true;
        frameid.Insert("ID3:", 0);
        metadata->Insert(frameid.Get(), strdup(frame_data.Get()));

        if (!stricmp(frameid.Get(), "ID3:PRIV:XMP") && frame_sz > 4)
        {
          UnpackXMPChunk(p+4, frame_sz-4, metadata);
        }
        else if (!stricmp(frameid.Get(), "ID3:PRIV:IXML") && frame_sz > 5)
        {
          UnpackIXMLChunk(p+5, frame_sz-5, metadata);
        }
      }
    }
    
    id3_pos += frame_sz;
  }
  WDL_ASSERT(id3_pos <= 10+tag_size); // did we run past the end?
#ifdef _DEBUG
  // the spec requires that any padding should be 0
  while (id3_pos < 10+tag_size)
  {
    WDL_ASSERT(!buf[id3_pos]);
    id3_pos++;
  }
#endif

  return has_tag;
}



int ReadMediaTags(WDL_FileRead *fr, WDL_StringKeyedArray<char*> *metadata,
  WDL_INT64 *_fstart, WDL_INT64 *_fend)
{
  if (!fr || !fr->IsOpen() || !metadata) return 0;

  int fstart=0;
  WDL_INT64 fend=fr->GetSize();

  WDL_HeapBuf hb;
  bool has_id3v1=false, has_id3v2=false, has_apev2=false;

  if (fend > 10)
  {
    unsigned char *buf=(unsigned char*)hb.Resize(128);
    fr->SetPosition(0);
    fr->Read(buf, 10);
    if (!memcmp(buf, "ID3" , 3) && buf[3] >= 2 && buf[3] <= 4 && buf[4] == 0)
    {
      int taglen=SynchSafeBytesToInt(true, (const char*)buf+6, 4);
      if (taglen && 10+taglen < fend)
      {
        buf=(unsigned char*)hb.Resize(10+taglen, false);
        fr->Read(buf+10, taglen);
        ParseID3v2((char*)buf, taglen, metadata, false, 0);
      }
      has_id3v2=true;
      fstart += taglen+10;
    }
  }

  if (fend-fstart > 32)
  {
    unsigned char *buf=(unsigned char*)hb.Resize(128);
    fr->SetPosition(fend-32);
    fr->Read(buf, 32);
    if (!memcmp(buf, "APETAGEX", 8) && _GetInt32LE(buf+8) == 2000)
    {
      int taglen=_GetInt32LE(buf+12); // includes footer but not header
      int tagcnt=_GetInt32LE(buf+16);
      int apelen=taglen+32; // including header and footer
      taglen -= 32; // not including header or footer
      if (taglen > 0 && tagcnt > 0 && fend-apelen > fstart)
      {
        fr->SetPosition(fend-apelen+32); // set to the end of the header
        buf=(unsigned char*)hb.Resize(taglen, false);
        fr->Read(buf, taglen);
        ParseAPEv2(buf, taglen, tagcnt, metadata);
      }
      has_apev2=true;
      fend -= apelen;
    }
  }

  // APEv2 could conceivably give a false positive for ID3v1
  if (fend-fstart > 128 && !has_apev2)
  {
    unsigned char *buf=(unsigned char*)hb.Resize(128);
    fr->SetPosition(fend-128);
    fr->Read(buf, 128);
    if (!memcmp(buf, "TAG", 3))
    {
      ParseID3v1((char*)buf, metadata, false);
      has_id3v1=true;
      fend -= 128;
    }
  }

  if (_fstart) *_fstart=fstart;
  if (_fend) *_fend=fend;

  return has_id3v1+has_id3v2+has_apev2;
}


#endif // _TAG_H_


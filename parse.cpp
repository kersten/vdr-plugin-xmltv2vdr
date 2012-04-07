/*
 * parse.cpp: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <locale.h>
#include <langinfo.h>
#include <time.h>
#include <pwd.h>
#include <iconv.h>
#include <vdr/timers.h>
#include <vdr/tools.h>
#include <sqlite3.h>

#include "xmltv2vdr.h"
#include "parse.h"

// -------------------------------------------------------

time_t cParse::ConvertXMLTVTime2UnixTime(char *xmltvtime)
{
    time_t offset=0;
    if (!xmltvtime) return (time_t) 0;
    char *withtz=strchr(xmltvtime,' ');
    int len;
    if (withtz)
    {
        len=strlen(xmltvtime)-(withtz-xmltvtime)-1;
        *withtz=':';
        if ((withtz[1]=='+') || (withtz[1]=='-'))
        {
            if (len==5)
            {
                int val=atoi(&withtz[1]);
                int h=val/100;
                int m=val-(h*100);
                offset=h*3600+m*60;
                setenv("TZ",":UTC",1);
            }
            else
            {
                setenv("TZ",":UTC",1);
            }
        }
        else
        {
            if (len>2)
            {
                setenv("TZ",withtz,1);
            }
            else
            {
                setenv("TZ",":UTC",1);
            }
        }
    }
    else
    {
        withtz=&xmltvtime[strlen(xmltvtime)];
        setenv("TZ",":UTC",1);
    }
    tzset();

    len=withtz-xmltvtime;
    if (len<4)
    {
        unsetenv("TZ");
        tzset();
        return (time_t) 0;
    }
    len-=2;
    char fmt[]="%Y%m%d%H%M%S";
    fmt[len]=0;

    struct tm tm;
    memset(&tm,0,sizeof(tm));
    if (!strptime(xmltvtime,fmt,&tm))
    {
        unsetenv("TZ");
        tzset();
        return (time_t) 0;
    }
    if (tm.tm_mday==0) tm.tm_mday=1;
    time_t ret=mktime(&tm);
    ret-=offset;
    unsetenv("TZ");
    tzset();
    return ret;
}

void cParse::RemoveNonAlphaNumeric(char *String)
{
    if (!String) return;

    // remove " Teil "
    int len=strlen(String);
    char *p=strstr(String," Teil ");
    if (p)
    {
        memmove(p,p+6,len-6);
    }

    // remove non alphanumeric characters
    len=strlen(String);
    p=String;
    int pos=0;
    while (*p)
    {
        // 0x30 - 0x39
        // 0x41 - 0x5A
        // 0x61 - 0x7A
        if ((*p<0x30) || (*p>0x7a) || (*p>0x39 && *p<0x41) || (*p>0x5A && *p< 0x61))
        {
            memmove(p,p+1,len-pos);
            len--;
            continue;
        }
        p++;
        pos++;
    }

    // remove leading numbers
    len=strlen(String);
    p=String;
    while (*p)
    {
        // 0x30 - 0x39
        if ((*p>=0x30) && (*p<=0x39))
        {
            memmove(p,p+1,len);
            len--;
            continue;
        }
        else
        {
            break;
        }
    }

    return;
}

bool cParse::FetchSeasonEpisode(iconv_t Conv, const char *EPDir, const char *Title,
                                const char *ShortText, int &Season, int &Episode)
{
    if (!EPDir) return false;
    if (!ShortText) return false;
    size_t slen=strlen(ShortText);
    if (!slen) return false;
    if (!Title) return false;
    if (Conv==(iconv_t) -1) return false;

    DIR *dir=opendir(EPDir);
    if (!dir) return false;
    struct dirent dirent_buf,*dirent;
    bool found=false;
    for (;;)
    {
        if (readdir_r(dir,&dirent_buf,&dirent)!=0) break;
        if (!dirent) break;
        if (dirent->d_name[0]=='.') continue;
        char *pt=strrchr(dirent->d_name,'.');
        if (pt) *pt=0;
        if (!strncasecmp(dirent->d_name,Title,strlen(dirent->d_name)))
        {
            found=true;
            break;
        }
    }
    closedir(dir);
    if (!found) return false;

    char *epfile=NULL;
    if (asprintf(&epfile,"%s/%s.episodes",EPDir,dirent->d_name)==-1) return false;

    FILE *f=fopen(epfile,"r");
    if (!f)
    {
        free(epfile);
        return false;
    }

    size_t dlen=4*slen;
    char *dshorttext=(char *) calloc(dlen,1);
    if (!dshorttext)
    {
        fclose(f);
        free(epfile);
        return false;
    }
    char *FromPtr=(char *) ShortText;
    char *ToPtr=(char *) dshorttext;

    if (iconv(Conv,&FromPtr,&slen,&ToPtr,&dlen)==(size_t) -1)
    {
        free(dshorttext);
        fclose(f);
        free(epfile);
        return false;
    }

    RemoveNonAlphaNumeric(dshorttext);
    if (!strlen(dshorttext))
    {
        free(dshorttext);
        fclose(f);
        free(epfile);
        return false;
    }

    char *line=NULL;
    size_t length;
    found=false;
    while (getline(&line,&length,f)!=-1)
    {
        if (line[0]=='#') continue;
        char epshorttext[256]="";
        char depshorttext[1024]="";
        if (sscanf(line,"%3d\t%3d\t%*3d\t%255c",&Season,&Episode,epshorttext)==3)
        {
            char *lf=strchr(epshorttext,'\n');
            if (lf) *lf=0;
            slen=strlen(epshorttext);
            dlen=sizeof(depshorttext);
            FromPtr=(char *) epshorttext;
            ToPtr=(char *) depshorttext;
            if (iconv(Conv,&FromPtr,&slen,&ToPtr,&dlen)!=(size_t) -1)
            {
                RemoveNonAlphaNumeric(depshorttext);
                if (!strncasecmp(dshorttext,depshorttext,strlen(depshorttext)))
                {
                    found=true;
                    break;
                }
            }
        }
    }

    if (!found)
    {
        isyslog("xmltv2vdr: failed to find '%s' for '%s' in eplists",ShortText,Title);
    }
    if (line) free(line);
    fclose(f);

    free(dshorttext);
    free(epfile);
    return found;
}

bool cParse::FetchEvent(xmlNodePtr enode)
{
    char *slang=getenv("LANG");
    xmlNodePtr node=enode->xmlChildrenNode;
    while (node)
    {
        if (node->type==XML_ELEMENT_NODE)
        {
            if ((!xmlStrcasecmp(node->name, (const xmlChar *) "title")))
            {
                xmlChar *lang=xmlGetProp(node,(const xmlChar *) "lang");
                xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                if (content)
                {
                    if (lang && slang && !xmlStrncasecmp(lang, (const xmlChar *) slang,2))
                    {
                        xevent.SetTitle((const char *) content);
                    }
                    else
                    {
                        if (!xevent.HasTitle())
                        {
                            xevent.SetTitle((const char *) content);
                        }
                        else
                        {
                            xevent.SetOrigTitle((const char *) content);
                        }
                    }
                    xmlFree(content);
                }
                if (lang) xmlFree(lang);
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "sub-title")))
            {
                // what to do with attribute lang?
                xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                if (content)
                {
                    xevent.SetShortText((const char *) content);
                    xmlFree(content);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "desc")))
            {
                // what to do with attribute lang?
                xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                if (content)
                {
                    xevent.SetDescription((const char *) content);
                    xmlFree(content);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "credits")))
            {
                xmlNodePtr vnode=node->xmlChildrenNode;
                while (vnode)
                {
                    if (vnode->type==XML_ELEMENT_NODE)
                    {
                        if ((!xmlStrcasecmp(vnode->name, (const xmlChar *) "actor")))
                        {
                            xmlChar *content=xmlNodeListGetString(vnode->doc,vnode->xmlChildrenNode,1);
                            if (content)
                            {
                                xmlChar *arole=xmlGetProp(node,(const xmlChar *) "actor role");
                                xevent.AddCredits((const char *) vnode->name,(const char *) content,(const char *) arole);
                                if (arole) xmlFree(arole);
                                xmlFree(content);
                            }
                        }
                        else
                        {
                            xmlChar *content=xmlNodeListGetString(vnode->doc,vnode->xmlChildrenNode,1);
                            if (content)
                            {
                                xevent.AddCredits((const char *) vnode->name,(const char *) content);
                                xmlFree(content);
                            }
                        }
                    }
                    vnode=vnode->next;
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "date")))
            {
                xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                if (content)
                {
                    xevent.SetYear(atoi((const char *) content));
                    xmlFree(content);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "category")))
            {
                // what to do with attribute lang?
                xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                if (content)
                {
                    if (isdigit(content[0]))
                    {
                        xevent.SetEventID(atoi((const char *) content));
                    }
                    else
                    {
                        xevent.AddCategory((const char *) content);
                    }
                    xmlFree(content);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "country")))
            {
                xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                if (content)
                {
                    xevent.SetCountry((const char *) content);
                    xmlFree(content);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "video")))
            {
                xmlNodePtr vnode=node->xmlChildrenNode;
                while (vnode)
                {
                    if (vnode->type==XML_ELEMENT_NODE)
                    {
                        if ((!xmlStrcasecmp(vnode->name, (const xmlChar *) "colour")))
                        {
                            xmlChar *content=xmlNodeListGetString(vnode->doc,vnode->xmlChildrenNode,1);
                            if (content)
                            {
                                xevent.AddVideo("colour",(const char *) content);
                                xmlFree(content);
                            }
                        }
                        if ((!xmlStrcasecmp(vnode->name, (const xmlChar *) "aspect")))
                        {
                            xmlChar *content=xmlNodeListGetString(vnode->doc,vnode->xmlChildrenNode,1);
                            if (content)
                            {
                                xevent.AddVideo("aspect",(const char *) content);
                                xmlFree(content);
                            }
                        }
                        if ((!xmlStrcasecmp(vnode->name, (const xmlChar *) "quality")))
                        {
                            xmlChar *content=xmlNodeListGetString(vnode->doc,vnode->xmlChildrenNode,1);
                            if (content)
                            {
                                xevent.AddVideo("quality",(const char *) content);
                                xmlFree(content);
                            }
                        }

                    }
                    vnode=vnode->next;
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "audio")))
            {
                xmlNodePtr vnode=node->xmlChildrenNode;
                while (vnode)
                {
                    if (vnode->type==XML_ELEMENT_NODE)
                    {
                        if ((!xmlStrcasecmp(vnode->name, (const xmlChar *) "stereo")))
                        {
                            xmlChar *content=xmlNodeListGetString(vnode->doc,vnode->xmlChildrenNode,1);
                            if (content)
                            {
                                content=(xmlChar*)strreplace((char *)content," ","");
                                xevent.SetAudio((const char *) content);
                                xmlFree(content);
                            }
                        }
                    }
                    vnode=vnode->next;
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "rating")))
            {
                xmlChar *system=xmlGetProp(node,(const xmlChar *) "system");
                if (system)
                {
                    xmlNodePtr vnode=node->xmlChildrenNode;
                    while (vnode)
                    {
                        if (vnode->type==XML_ELEMENT_NODE)
                        {
                            if ((!xmlStrcasecmp(vnode->name, (const xmlChar *) "value")))
                            {
                                xmlChar *content=xmlNodeListGetString(vnode->doc,vnode->xmlChildrenNode,1);
                                if (content)
                                {
                                    xevent.AddRating((const char *) system,(const char *) content);
                                    xmlFree(content);
                                }
                            }
                        }
                        vnode=vnode->next;
                    }
                    xmlFree(system);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "star-rating")))
            {
                xmlChar *system=xmlGetProp(node,(const xmlChar *) "system");
                xmlNodePtr vnode=node->xmlChildrenNode;
                while (vnode)
                {
                    if (vnode->type==XML_ELEMENT_NODE)
                    {
                        if ((!xmlStrcasecmp(vnode->name, (const xmlChar *) "value")))
                        {
                            xmlChar *content=xmlNodeListGetString(vnode->doc,vnode->xmlChildrenNode,1);
                            if (content)
                            {
                                xevent.AddStarRating((const char *) system,(const char *) content);
                                xmlFree(content);
                            }
                        }
                    }
                    vnode=vnode->next;
                }
                if (system) xmlFree(system);
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "review")))
            {
                xmlChar *type=xmlGetProp(node,(const xmlChar *) "type");
                if (type && !xmlStrcasecmp(type, (const xmlChar *) "text"))
                {
                    xmlChar *content=xmlNodeListGetString(node->doc,node->xmlChildrenNode,1);
                    if (content)
                    {
                        xevent.AddReview((const char *) content);
                        xmlFree(content);
                    }
                    xmlFree(type);
                }
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "icon")))
            {
                // http-link inside -> just ignore
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "length")))
            {
                // length without advertisements -> just ignore
            }
            else if ((!xmlStrcasecmp(node->name, (const xmlChar *) "episode-num")))
            {
                // episode-num in not usable format -> just ignore
            }
            else
            {
                source->Elog("unknown element %s, please report!",node->name);
            }
        }
        node=node->next;
    }

    int season,episode;
    if (FetchSeasonEpisode(conv,epdir,xevent.Title(),xevent.ShortText(),season,episode))
    {
        xevent.SetSeason(season);
        xevent.SetEpisode(episode);
    }

    return xevent.HasTitle();
}

int cParse::Process(cEPGExecutor &myExecutor,char *buffer, int bufsize)
{
    if (!buffer) return 134;
    if (!bufsize) return 134;

    xmlDocPtr xmltv;
    xmltv=xmlReadMemory(buffer,bufsize,NULL,NULL,0);
    if (!xmltv)
    {
        source->Elog("failed to parse xmltv");
        return 141;
    }

    xmlNodePtr rootnode=xmlDocGetRootElement(xmltv);
    if (!rootnode)
    {
        source->Elog("no rootnode in xmltv");
        xmlFreeDoc(xmltv);
        return 141;
    }

    sqlite3 *db=NULL;
    if (sqlite3_open(source->EPGFile(),&db)!=SQLITE_OK)
    {
        source->Elog("failed to open or create %s",source->EPGFile());
        xmlFreeDoc(xmltv);
        return 141;
    }

    char sql[]="CREATE TABLE IF NOT EXISTS epg (" \
               "src nvarchar(100), channelid nvarchar(255), eventid int, eiteventid int, "\
               "starttime datetime, duration int, title nvarchar(255),origtitle nvarchar(255), "\
               "shorttext nvarchar(255), description text, eitdescription text, " \
               "country nvarchar(255), year int, " \
               "credits text, category text, review text, rating text, " \
               "starrating text, video text, audio text, season int, episode int, mixing int," \
               "srcidx int," \
               "PRIMARY KEY(src, channelid, eventid)" \
               ")";

    char *errmsg;
    if (sqlite3_exec(db,sql,NULL,NULL,&errmsg)!=SQLITE_OK)
    {
        source->Elog("createdb: %s",errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(db);
        xmlFreeDoc(xmltv);
        return 141;
    }

    time_t begin=time(NULL)-7200;
    xmlNodePtr node=rootnode->xmlChildrenNode;

    int lerr=0;
    while (node)
    {
        if (node->type!=XML_ELEMENT_NODE)
        {
            node=node->next;
            continue;
        }
        if ((xmlStrcasecmp(node->name, (const xmlChar *) "programme")))
        {
            node=node->next;
            continue;
        }
        xmlChar *channelid=xmlGetProp(node,(const xmlChar *) "channel");
        if (!channelid)
        {
            if (lerr!=PARSE_NOCHANNELID)
                source->Elog("missing channelid in xmltv file");
            lerr=PARSE_NOCHANNELID;
            node=node->next;
            continue;
        }
        cEPGMapping *map=maps->GetMap((const char *) channelid);
        if (!map)
        {
            if (lerr!=PARSE_NOMAPPING)
                source->Elog("no mapping for channelid %s",channelid);
            lerr=PARSE_NOMAPPING;
            xmlFree(channelid);
            node=node->next;
            continue;
        }

        xmlChar *start,*stop;
        time_t starttime=(time_t) 0;
        time_t stoptime=(time_t) 0;
        start=xmlGetProp(node,(const xmlChar *) "start");
        if (start)
        {
            starttime=ConvertXMLTVTime2UnixTime((char *) start);
            if (starttime)
            {
                stop=xmlGetProp(node,(const xmlChar *) "stop");
                if (stop)
                {
                    stoptime=ConvertXMLTVTime2UnixTime((char *) stop);
                    xmlFree(stop);
                }
            }
            xmlFree(start);
        }

        if (!starttime)
        {
            if (lerr!=PARSE_XMLTVERR)
                source->Elog("no starttime, check xmltv file");
            lerr=PARSE_XMLTVERR;
            xmlFree(channelid);
            node=node->next;
            continue;
        }

        if (starttime<begin)
        {
            xmlFree(channelid);
            node=node->next;
            continue;
        }
        xevent.Clear();
        xevent.SetStartTime(starttime);
        if (stoptime) xevent.SetDuration(stoptime-starttime);

        if ((map->Flags() & OPT_APPEND)!=OPT_APPEND) xevent.SetMixing();

        if (!FetchEvent(node)) // sets xevent
        {
            source->Dlog("failed to fetch event");
            node=node->next;
            xmlFree(channelid);
            continue;
        }

        char *errmsg;
        const char *sql=xevent.GetSQL(source->Name(),source->Index(),(const char *) channelid);
        if (sqlite3_exec(db,sql,NULL,NULL,&errmsg)!=SQLITE_OK)
        {
            source->Elog("sqlite3: %s",errmsg);
            sqlite3_free(errmsg);
            xmlFree(channelid);
            break;
        }
        xmlFree(channelid);

        node=node->next;
        if (!myExecutor.StillRunning())
        {
            source->Ilog("request to stop from vdr");
            break;
        }
    }
    sqlite3_close(db);

    xmlFreeDoc(xmltv);
    return 0;
}

void cParse::InitLibXML()
{
    xmlInitParser();
}

void cParse::CleanupLibXML()
{
    xmlCleanupParser();
}

cParse::cParse(cEPGSource *Source, cEPGMappings *Maps)
{
    source=Source;
    maps=Maps;

    struct passwd pwd,*pwdbuf;
    char buf[1024];
    getpwuid_r(getuid(),&pwd,buf,sizeof(buf),&pwdbuf);
    if (pwdbuf)
    {
        if (asprintf(&epdir,"%s/.eplists/lists",pwdbuf->pw_dir)!=-1)
        {
            if (access(epdir,R_OK))
            {
                free(epdir);
                epdir=NULL;
            }
            else
            {
                conv=iconv_open("US-ASCII//TRANSLIT","UTF-8");
            }
        }
        else
        {
            epdir=NULL;
        }
    }
}

cParse::~cParse()
{
    if (epdir)
    {
        free(epdir);
        iconv_close(conv);
    }
}

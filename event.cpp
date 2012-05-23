/*
 * event.cpp: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <vdr/tools.h>
#include <pcrecpp.h>
#include "event.h"

extern char *strcatrealloc(char *, const char*);

cXMLTVStringList::~cXMLTVStringList(void)
{
    free(buf);
    Clear();
}

void cXMLTVStringList::Clear(void)
{
    for (int i=0; i<Size();i++)
        free(At(i));
    cVector< char* >::Clear();
}

const char* cXMLTVStringList::toString()
{
    free(buf);
    buf=NULL;

    for (int i=0; i<Size();i++)
    {
        buf=strcatrealloc(buf,operator[](i));
        if (i<Size()-1) buf=strcatrealloc(buf,"@");
    }

    if (!buf) if (asprintf(&buf,"NULL")==-1) buf=NULL;
    return buf;
}

// -------------------------------------------------------------

void cXMLTVEvent::SetSource(const char *Source)
{
    source = strcpyrealloc(source, Source);
    if (source) source = compactspace(source);
}

void cXMLTVEvent::SetChannelID(const char *ChannelID)
{
    channelid = strcpyrealloc(channelid, ChannelID);
    if (channelid) channelid = compactspace(channelid);
}

void cXMLTVEvent::SetTitle(const char *Title)
{
    title = strcpyrealloc(title, Title);
    if (title) title = compactspace(title);
}

void cXMLTVEvent::SetOrigTitle(const char *OrigTitle)
{
    origtitle = strcpyrealloc(origtitle, OrigTitle);
    if (origtitle) origtitle=compactspace(origtitle);
}

void cXMLTVEvent::SetShortText(const char *ShortText)
{
    shorttext=strcpyrealloc(shorttext,ShortText);
    if (shorttext) shorttext=compactspace(shorttext);
}

void cXMLTVEvent::SetDescription(const char *Description)
{
    description = strcpyrealloc(description, Description);
    if (description) description = compactspace(description);
}

void cXMLTVEvent::SetEITDescription(const char *EITDescription)
{
    eitdescription = strcpyrealloc(eitdescription, EITDescription);
    if (eitdescription) eitdescription = compactspace(eitdescription);
}

void cXMLTVEvent::SetCountry(const char *Country)
{
    country=strcpyrealloc(country, Country);
    if (country) country=compactspace(country);
}

void cXMLTVEvent::SetAudio(const char *Audio)
{
    audio=strcpyrealloc(audio, Audio);
    if (audio) audio = compactspace(audio);
}

void cXMLTVEvent::SetCredits(const char *Credits)
{
    if (!Credits) return;
    char *c=strdup(Credits);
    if (!c) return;
    char *sp,*tok;
    char delim[]="@";
    tok=strtok_r(c,delim,&sp);
    while (tok)
    {
        credits.Append(strdup(tok));
        tok=strtok_r(NULL,delim,&sp);
    }
    credits.Sort();
    free(c);
}

void cXMLTVEvent::SetCategory(const char *Category)
{
    if (!Category) return;
    char *c=strdup(Category);
    if (!c) return;
    char *sp,*tok;
    char delim[]="@";
    tok=strtok_r(c,delim,&sp);
    while (tok)
    {
        category.Append(strdup(tok));
        tok=strtok_r(NULL,delim,&sp);
    }
    category.Sort();
    free(c);
}

void cXMLTVEvent::SetReview(const char *Review)
{
    if (!Review) return;
    char *c=strdup(Review);
    if (!c) return;
    char *sp,*tok;
    char delim[]="@";
    tok=strtok_r(c,delim,&sp);
    while (tok)
    {
        review.Append(strdup(tok));
        tok=strtok_r(NULL,delim,&sp);
    }
    free(c);
}

void cXMLTVEvent::SetRating(const char *Rating)
{
    if (!Rating) return;
    char *c=strdup(Rating);
    if (!c) return;
    char *sp,*tok;
    char delim[]="@";
    tok=strtok_r(c,delim,&sp);
    while (tok)
    {
        rating.Append(strdup(tok));
        char *rval=strchr(tok,'|');
        if (rval)
        {
            rval++;
            int r=atoi(rval);
            if ((r>0 && r<=18) && (r>parentalRating)) parentalRating=r;
        }
        tok=strtok_r(NULL,delim,&sp);
    }
    rating.Sort();
    free(c);
}

void cXMLTVEvent::SetVideo(const char *Video)
{
    if (!Video) return;
    char *c=strdup(Video);
    if (!c) return;
    char *sp,*tok;
    char delim[]="@";
    tok=strtok_r(c,delim,&sp);
    while (tok)
    {
        video.Append(strdup(tok));
        tok=strtok_r(NULL,delim,&sp);
    }
    free(c);
}

void cXMLTVEvent::SetPics(const char* Pics)
{
    if (!Pics) return;
    char *c=strdup(Pics);
    if (!c) return;
    char *sp,*tok;
    char delim[]="@";
    tok=strtok_r(c,delim,&sp);
    while (tok)
    {
        pics.Append(strdup(tok));
        tok=strtok_r(NULL,delim,&sp);
    }
    free(c);
}

void cXMLTVEvent::SetStarRating(const char *StarRating)
{
    if (!StarRating) return;
    char *c=strdup(StarRating);
    if (!c) return;
    char *sp,*tok;
    char delim[]="@";
    tok=strtok_r(c,delim,&sp);
    while (tok)
    {
        starrating.Append(strdup(tok));
        tok=strtok_r(NULL,delim,&sp);
    }
    starrating.Sort();
    free(c);
}

void cXMLTVEvent::AddReview(const char *Review)
{
    review.Append(compactspace(strdup(Review)));
}

void cXMLTVEvent::AddPics(const char* Pic)
{
    pics.Append(compactspace(strdup(Pic)));
}

void cXMLTVEvent::AddVideo(const char *VType, const char *VContent)
{
    char *value=NULL;
    if (asprintf(&value,"%s|%s",VType,VContent)==-1) return;
    video.Append(value);
}

void cXMLTVEvent::AddRating(const char *System, const char *Rating)
{
    char *value=NULL;
    if (asprintf(&value,"%s|%s",System,Rating)==-1) return;
    int r=atoi(Rating);
    if ((r>0 && r<=18) && (r>parentalRating)) parentalRating=r;
    rating.Append(value);
    rating.Sort();
}

void cXMLTVEvent::AddStarRating(const char *System, const char *Rating)
{
    char *value=NULL;
    if (System)
    {
        if (asprintf(&value,"%s|%s",System,Rating)==-1) return;
    }
    else
    {
        if (asprintf(&value,"*|%s",Rating)==-1) return;
    }
    starrating.Append(value);
}

void cXMLTVEvent::AddCategory(const char *Category)
{
    category.Append(compactspace(strdup(Category)));
    category.Sort();
}

void cXMLTVEvent::AddCredits(const char *CreditType, const char *Credit, const char *Addendum)
{
    char *value=NULL;
    if (Addendum)
    {
        if (asprintf(&value,"%s|%s (%s)",CreditType,Credit,Addendum)==-1) return;
    }
    else
    {
        if (asprintf(&value,"%s|%s",CreditType,Credit)==-1) return;
    }
    credits.Append(value);
    credits.Sort();
}

void cXMLTVEvent::CreateEventID(time_t StartTime)
{
    if (eventid) return;
    // create own 16bit eventid
    struct tm tm;
    if (!localtime_r(&StartTime,&tm)) return;

    // this id cycles every 31 days, so if we have
    // 4 weeks programme in advance the id will
    // occupy already existing entries
    // till now, i'm only aware of 2 weeks
    // programme in advance
    int newid=((tm.tm_mday) & 0x1F)<<11;
    newid|=((tm.tm_hour & 0x1F)<<6);
    newid|=(tm.tm_min & 0x3F);

    eventid=newid & 0xFFFF;
}

void cXMLTVEvent::GetSQL(const char *Source, int SrcIdx, const char *ChannelID, char **Insert, char **Update)
{
    if (sql_insert)
    {
        free(sql_insert);
        sql_insert=NULL;
    }
    if (sql_update)
    {
        free(sql_update);
        sql_update=NULL;
    }

    if (!Insert) return;
    if (!Update) return;

    *Insert=NULL;
    *Update=NULL;

    const char *cr=credits.toString();
    const char *ca=category.toString();
    const char *re=review.toString();
    const char *ra=rating.toString();
    const char *sr=starrating.toString();
    const char *vi=video.toString();
    const char *pi=pics.toString();

    if (!eventid) return;

    if (asprintf(&sql_insert,
                 "INSERT OR FAIL INTO epg (src,channelid,eventid,starttime,duration,"\
                 "title,origtitle,shorttext,description,country,year,credits,category,"\
                 "review,rating,starrating,video,audio,season,episode,episodeoverall,pics,srcidx) "\
                 "VALUES (^%s^,^%s^,%u,%li,%i,"\
                 "^%s^,^%s^,^%s^,^%s^,^%s^,%i,^%s^,^%s^,"\
                 "^%s^,^%s^,^%s^,^%s^,^%s^,%i,%i,%i,^%s^,%i);"
                 ,
                 Source,ChannelID,eventid,starttime,duration,title,
                 origtitle ? origtitle : "NULL",
                 shorttext ? shorttext : "NULL",
                 description ? description : "NULL",
                 country ? country : "NULL",
                 year,
                 cr,ca,re,ra,sr,vi,
                 audio ? audio : "NULL",
                 season, episode, episodeoverall, pi, SrcIdx
                )==-1)
    {
        sql_insert=NULL;
        return;
    }

    if (asprintf(&sql_update,
                 "UPDATE epg SET duration=%i,starttime=%li,title=^%s^,origtitle=^%s^,"\
                 "shorttext=^%s^,description=^%s^,country=^%s^,year=%i,credits=^%s^,category=^%s^,"\
                 "review=^%s^,rating=^%s^,starrating=^%s^,video=^%s^,audio=^%s^,season=%i,episode=%i, "\
                 "episodeoverall=%i,pics=^%s^,srcidx=%i " \
                 " where src=^%s^ and channelid=^%s^ and eventid=%u"
                 ,
                 duration,starttime,title,
                 origtitle ? origtitle : "NULL",
                 shorttext ? shorttext : "NULL",
                 description ? description : "NULL",
                 country ? country : "NULL",
                 year,
                 cr,ca,re,ra,sr,vi,
                 audio ? audio : "NULL",
                 season, episode, episodeoverall, pi, SrcIdx,
                 Source,ChannelID,eventid
                )==-1)
    {
        sql_update=NULL;
        return;
    }

    string si=sql_insert;
    int ireps;
    ireps=pcrecpp::RE("'").GlobalReplace("''",&si);
    ireps+=pcrecpp::RE("\\^").GlobalReplace("'",&si);
    ireps+=pcrecpp::RE("'NULL'").GlobalReplace("NULL",&si);
    if (ireps)
    {
        sql_insert=(char *) realloc(sql_insert,si.size()+1);
        strcpy(sql_insert,si.c_str());
    }
    *Insert=sql_insert;

    string su=sql_update;
    int ureps;
    ureps=pcrecpp::RE("'").GlobalReplace("''",&su);
    ureps+=pcrecpp::RE("\\^").GlobalReplace("'",&su);
    ureps+=pcrecpp::RE("'NULL'").GlobalReplace("NULL",&su);
    if (ureps)
    {
        sql_update=(char *) realloc(sql_update,su.size()+1);
        strcpy(sql_update,su.c_str());
    }
    *Update=sql_update;
}

void cXMLTVEvent::Clear()
{
    if (source)
    {
        free(source);
        source=NULL;
    }
    if (sql_insert)
    {
        free(sql_insert);
        sql_insert=NULL;
    }
    if (sql_update)
    {
        free(sql_update);
        sql_update=NULL;
    }
    if (title)
    {
        free(title);
        title=NULL;
    }
    if (shorttext)
    {
        free(shorttext);
        shorttext=NULL;
    }
    if (description)
    {
        free(description);
        description=NULL;
    }
    if (eitdescription)
    {
        free(eitdescription);
        eitdescription=NULL;
    }
    if (country)
    {
        free(country);
        country=NULL;
    }
    if (origtitle)
    {
        free(origtitle);
        origtitle=NULL;
    }
    if (audio)
    {
        free(audio);
        audio=NULL;
    }
    if (channelid)
    {
        free(channelid);
        channelid=NULL;
    }
    year=0;
    starttime = 0;
    duration = 0;
    eventid=eiteventid=0;
    video.Clear();
    credits.Clear();
    category.Clear();
    review.Clear();
    rating.Clear();
    starrating.Clear();
    pics.Clear();
    season=0;
    episode=0;
    episodeoverall=0;
    parentalRating=0;
    memset(&contents,0,sizeof(contents));
}

cXMLTVEvent::cXMLTVEvent()
{
    sql_insert=NULL;
    sql_update=NULL;
    source=NULL;
    channelid=NULL;
    title=NULL;
    shorttext=NULL;
    description=eitdescription=NULL;
    country=NULL;
    origtitle=NULL;
    audio=NULL;
    Clear();
}

cXMLTVEvent::~cXMLTVEvent()
{
    Clear();
}

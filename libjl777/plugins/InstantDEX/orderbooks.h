//
//  orderbooks.h
//
//  Created by jl777 on 7/9/14.
//  Copyright (c) 2014 jl777. All rights reserved.
//

#ifndef xcode_orderbooks_h
#define xcode_orderbooks_h

void testfoo(char *str)
{
    int32_t i,j;
    for (i=0; i<BUNDLE.num; i++)
    {
        if ( BUNDLE.ptrs[i]->basketsize == 0 )
            continue;
        for (j=0; j<BUNDLE.ptrs[i]->basketsize; j++)
            printf("%d ",BUNDLE.ptrs[i]->basket[j].groupsize);
        printf("(%s) groupsizes basketsize.%d\n",str,BUNDLE.ptrs[i]->basketsize);
    }
}

struct prices777 *prices777_createbasket(char *name,char *base,char *rel,uint64_t baseid,uint64_t relid,struct prices777_basket *basket,int32_t n)
{
    int32_t i,j,m,max = 0; double firstwt,wtsum; struct prices777 *prices,*feature;
    if ( n > MAX_GROUPS )
    {
        printf("baskets limited to %d, %d is too many for %s.(%s/%s)\n",MAX_GROUPS,n,name,base,rel);
        return(0);
    }
    prices = prices777_initpair(1,0,"basket",base,rel,0.,name,baseid,relid,n);
    for (i=0; i<n; i++)
    {
        feature = basket[i].prices;
        feature->dependents = realloc(feature->dependents,sizeof(*feature->dependents) * (feature->numdependents + 1));
        feature->dependents[feature->numdependents++] = &prices->changed;
        if ( basket[i].groupid > max )
            max = basket[i].groupid;
        if ( fabs(basket[i].wt) < SMALLVAL )
        {
            printf("all basket features must have nonzero wt\n");
            free(prices);
            return(0);
        }
    }
    prices->numgroups = (max + 1);
    for (j=0; j<prices->numgroups; j++)
    {
        for (firstwt=i=m=0; i<n; i++)
        {
            //printf("i.%d groupid.%d wt %f m.%d\n",i,basket[i].groupid,basket[i].wt,m);
            if ( basket[i].groupid == j )
            {
                if ( firstwt == 0. )
                    prices->groupwts[j] = firstwt = basket[i].wt;
                else if ( basket[i].wt != firstwt )
                {
                    printf("warning features of same group.%d different wt: %d %f != %f\n",j,i,firstwt,basket[i].wt);
                    // free(prices);
                    // return(0);
                }
                //prices->basket[prices->basketsize++] = basket[i];
                m++;
            }
        }
        //printf("m.%d\n",m);
        for (i=0; i<n; i++)
            if ( basket[i].groupid == j )
                basket[i].groupsize = m, printf("basketsize.%d n.%d j.%d groupsize[%d] <- m.%d\n",prices->basketsize,n,j,i,m);
    }
    for (j=0; j<prices->numgroups; j++)
        for (i=0; i<n; i++)
            if ( basket[i].groupid == j )
                prices->basket[prices->basketsize++] = basket[i];
    for (i=-1; i<=1; i+=2)
    {
        for (wtsum=j=m=0; j<prices->numgroups; j++)
        {
            if ( prices->groupwts[j]*i > 0 )
                wtsum += prices->groupwts[j], m++;
        }
        if ( wtsum != 0. )
        {
            if ( wtsum < 0. )
                wtsum = -wtsum;
            for (j=0; j<prices->numgroups; j++)
                prices->groupwts[j] /= wtsum;
        }
    }
    for (j=0; j<prices->numgroups; j++)
        printf("%9.6f ",prices->groupwts[j]);
    printf("groupwts\n");
    return(prices);
}

double prices777_price_volume(double *volumep,uint64_t baseamount,uint64_t relamount)
{
    *volumep = (((double)baseamount + 0.000000009999999) / SATOSHIDEN);
    if ( baseamount > 0. )
        return((double)relamount / (double)baseamount);
    else return(0.);
}

void prices777_best_amounts(uint64_t *baseamountp,uint64_t *relamountp,double price,double volume)
{
    double checkprice,checkvol,distA,distB,metric,bestmetric = (1. / SMALLVAL);
    uint64_t baseamount,relamount,bestbaseamount = 0,bestrelamount = 0;
    int32_t i,j;
    baseamount = volume * SATOSHIDEN;
    relamount = ((price * volume) * SATOSHIDEN);
    //*baseamountp = baseamount, *relamountp = relamount;
    //return;
    for (i=-1; i<=1; i++)
        for (j=-1; j<=1; j++)
        {
            checkprice = prices777_price_volume(&checkvol,baseamount+i,relamount+j);
            distA = (checkprice - price);
            distA *= distA;
            distB = (checkvol - volume);
            distB *= distB;
            metric = sqrt(distA + distB);
            if ( metric < bestmetric )
            {
                bestmetric = metric;
                bestbaseamount = baseamount + i;
                bestrelamount = relamount + j;
                //printf("i.%d j.%d metric. %f\n",i,j,metric);
            }
        }
    *baseamountp = bestbaseamount;
    *relamountp = bestrelamount;
}

void prices777_additem(cJSON **highbidp,cJSON **lowaskp,cJSON *bids,cJSON *asks,int32_t ind,cJSON *item,int32_t bidask)
{
    if ( bidask == 0 )
    {
        cJSON_AddItemToArray(bids,item);
        if ( ind == 0 )
            *highbidp = item;
    }
    else
    {
        cJSON_AddItemToArray(asks,item);
        if ( ind == 0 )
            *lowaskp = item;
    }
}

cJSON *prices777_item(struct prices777 *prices,int32_t bidask,double price,double volume,double wt,uint64_t orderid)
{
    cJSON *item;
     item = cJSON_CreateObject();
    jaddnum(item,"price",price);
    jaddnum(item,"volume",volume);
    if ( wt < 0 )
    {
        bidask ^= 1;
        volume *= price;
        price = 1./price;
    }
    jaddstr(item,"trade",bidask == 0 ? "sell":"buy");
    jaddstr(item,"exchange",prices->exchange);
    jaddstr(item,"name",prices->contract);
    if ( orderid != 0 )
        jadd64bits(item,"orderid",orderid);
    if ( wt < 0 )
    {
        jaddnum(item,"orderprice",price);
        jaddnum(item,"ordervolume",volume);
    }
    return(item);
}

cJSON *prices777_tradeitem(struct prices777 *prices,int32_t bidask,int32_t slot,uint32_t timestamp,double price,double volume,double wt,uint64_t orderid)
{
    static uint32_t match,error;
    if ( prices->O.timestamp == timestamp )
    {
        //printf("tradeitem.(%s %f %f)\n",prices->exchange,price,volume);
        if ( bidask == 0 && prices->O.book[MAX_GROUPS][slot].bid.price == price && prices->O.book[MAX_GROUPS][slot].bid.vol == volume )
            match++;
        else if ( bidask != 0 && prices->O.book[MAX_GROUPS][slot].ask.price == price && prices->O.book[MAX_GROUPS][slot].ask.vol == volume )
            match++;
    }
    else if ( prices->O2.timestamp == timestamp )
    {
        //printf("2tradeitem.(%s %f %f)\n",prices->exchange,price,volume);
        if ( bidask == 0 && prices->O2.book[MAX_GROUPS][slot].bid.price == price && prices->O2.book[MAX_GROUPS][slot].bid.vol == volume )
            match++;
        else if ( bidask != 0 && prices->O2.book[MAX_GROUPS][slot].ask.price == price && prices->O2.book[MAX_GROUPS][slot].ask.vol == volume )
            match++;
    } else error++, printf("mismatched tradeitem error.%d match.%d\n",error,match);
    return(prices777_item(prices,bidask,price,volume,wt,orderid));
}

cJSON *prices777_tradesequence(struct prices777 *prices,struct prices777_basketinfo *OB,int32_t slot,int32_t bidask,double refwt)
{
    int32_t i,ind,srcbidask,err = 0; uint64_t orderid; double price,volume,wt; cJSON *array; uint32_t timestamp;
    struct prices777_orderentry *gp; struct prices777 *src;
    array = cJSON_CreateArray();
    for (i=0; i<prices->numgroups; i++)
    {
        gp = &OB->book[i][slot];
        if ( bidask == 0 )
            src = gp->bid.source, timestamp = gp->bid.timestamp, ind = gp->bid.ind, price = gp->bid.price, volume = gp->bid.vol, wt = gp->bid.wt, orderid = gp->bid.orderid;
        else src = gp->ask.source, timestamp = gp->ask.timestamp, ind = gp->ask.ind, price = gp->ask.price, volume = gp->ask.vol, wt = gp->ask.wt, orderid = gp->ask.orderid;
        srcbidask = (ind & 1), ind >>= 1;
        if ( src != 0 )
        {
            if ( src->basketsize == 0 )
                jaddi(array,prices777_tradeitem(src,srcbidask,ind,timestamp,price,volume,refwt*wt,orderid));
            else if ( src->O.timestamp == timestamp )
                jaddi(array,prices777_tradesequence(src,&src->O,ind,srcbidask,refwt*wt));
            else if ( src->O2.timestamp == timestamp )
                jaddi(array,prices777_tradesequence(src,&src->O2,ind,srcbidask,refwt*wt));
            else err =  1;
        }
        if ( src == 0 || err != 0 )
        {
            //jaddi(array,prices777_item(prices,bidask,price,volume,wt,orderid));
            printf("prices777_tradesequence warning cant match timestamp %u (%s %s/%s)\n",timestamp,prices->contract,prices->base,prices->rel);
        }
    }
    return(array);
}

void prices777_orderbook_item(struct prices777 *prices,struct prices777_basketinfo *OB,int32_t slot,int32_t bidask,cJSON *array,int32_t invert,int32_t allflag,double price,double volume,double oppoprice,double oppovol)
{
    cJSON *item;
    item = cJSON_CreateObject();
    if ( invert != 0 )
    {
        price = (1. / oppoprice);
        volume = (oppoprice * oppovol);
    }
    jaddnum(item,"price",price), jaddnum(item,"volume",volume);
    if ( allflag != 0 )
    {
        if ( prices->basketsize == 0 )
            jaddstr(item,"exchange",prices->exchange);
        else jadd(item,"trades",prices777_tradesequence(prices,OB,slot,bidask,1.));
    }
    jaddi(array,item);
}

char *prices777_orderbook_jsonstr(int32_t invert,uint64_t nxt64bits,struct prices777 *prices,struct prices777_basketinfo *OB,int32_t maxdepth,int32_t allflag)
{
    struct prices777_orderentry *gp; cJSON *json,*bids,*asks,*array;  int32_t i,n; char baserel[64],assetA[64],assetB[64],NXTaddr[64];
    if ( invert == 0 )
        sprintf(baserel,"%s/%s",prices->base,prices->rel);
    else sprintf(baserel,"%s/%s",prices->rel,prices->base);
    if ( Debuglevel > 2 )
        printf("ORDERBOOK %s/%s iQsize.%ld numbids.%d numasks.%d maxdepth.%d (%llu %llu)\n",prices->base,prices->rel,sizeof(struct InstantDEX_quote),OB->numbids,OB->numasks,maxdepth,(long long)prices->baseid,(long long)prices->relid);
    json = cJSON_CreateObject(), bids = cJSON_CreateArray(), asks = cJSON_CreateArray();
    n = (invert == 0) ? OB->numbids : OB->numasks;
    if ( n > 0 )
    {
        array = (invert == 0) ? bids : asks;
        gp = &OB->book[MAX_GROUPS][0];
        for (i=0; i<n && i<maxdepth; i++,gp++)
            prices777_orderbook_item(prices,OB,i,invert,array,invert,allflag,gp->bid.price,gp->bid.vol,gp->ask.price,gp->ask.vol);
    }
    n = (invert == 0) ? OB->numasks : OB->numbids;
    if ( n > 0 )
    {
        array = (invert == 0) ? asks : bids;
        gp = &OB->book[MAX_GROUPS][0];
        for (i=0; i<n && i<maxdepth; i++,gp++)
            prices777_orderbook_item(prices,OB,i,!invert,array,invert,allflag,gp->ask.price,gp->ask.vol,gp->bid.price,gp->bid.vol);
    }
    expand_nxt64bits(NXTaddr,nxt64bits);
    expand_nxt64bits(assetA,invert==0 ? prices->baseid : prices->relid);
    expand_nxt64bits(assetB,invert!=0 ? prices->baseid : prices->relid);
    cJSON_AddItemToObject(json,"inverted",cJSON_CreateNumber(invert));
    cJSON_AddItemToObject(json,"pair",cJSON_CreateString(baserel));
    cJSON_AddItemToObject(json,"baseid",cJSON_CreateString(assetA));
    cJSON_AddItemToObject(json,"relid",cJSON_CreateString(assetB));
    cJSON_AddItemToObject(json,"bids",bids);
    cJSON_AddItemToObject(json,"asks",asks);
    cJSON_AddItemToObject(json,"numbids",cJSON_CreateNumber(OB->numbids));
    cJSON_AddItemToObject(json,"numasks",cJSON_CreateNumber(OB->numasks));
    cJSON_AddItemToObject(json,"lastbid",cJSON_CreateNumber(prices->lastbid));
    cJSON_AddItemToObject(json,"lastask",cJSON_CreateNumber(prices->lastask));
    cJSON_AddItemToObject(json,"NXT",cJSON_CreateString(NXTaddr));
    cJSON_AddItemToObject(json,"timestamp",cJSON_CreateNumber(time(NULL)));
    cJSON_AddItemToObject(json,"maxdepth",cJSON_CreateNumber(maxdepth));
    return(jprint(json,1));
}

void prices777_jsonstrs(struct prices777 *prices,struct prices777_basketinfo *OB)
{
    int32_t allflag; char *strs[4];
    for (allflag=0; allflag<4; allflag++)
    {
        strs[allflag] = prices777_orderbook_jsonstr(allflag/2,SUPERNET.my64bits,prices,OB,MAX_DEPTH,allflag%2);
        if ( Debuglevel > 2 )
            printf("strs[%d].(%s)\n",allflag,strs[allflag]);
    }
    portable_mutex_lock(&prices->mutex);
    for (allflag=0; allflag<4; allflag++)
    {
        if ( prices->orderbook_jsonstrs[allflag/2][allflag%2] != 0 )
            free(prices->orderbook_jsonstrs[allflag/2][allflag%2]);
        prices->orderbook_jsonstrs[allflag/2][allflag%2] = strs[allflag];
    }
    portable_mutex_unlock(&prices->mutex);
}

void prices777_json_quotes(double *hblap,struct prices777 *prices,cJSON *bids,cJSON *asks,int32_t maxdepth,char *pricefield,char *volfield,uint32_t reftimestamp)
{
    cJSON *item; int32_t i,slot,n,m,dir,bidask,numitems; uint32_t timestamp; double price,volume,hbla = 0.;
    struct prices777_basketinfo OB; struct prices777_orderentry *gp;
    memset(&OB,0,sizeof(OB));
    if ( reftimestamp == 0 )
        reftimestamp = (uint32_t)time(NULL);
    OB.timestamp = reftimestamp;
    n = cJSON_GetArraySize(bids);
    if ( maxdepth != 0 && n > maxdepth )
        n = maxdepth;
    m = cJSON_GetArraySize(asks);
    if ( maxdepth != 0 && m > maxdepth )
        m = maxdepth;
    for (i=0; i<n||i<m; i++)
    {
        gp = &OB.book[MAX_GROUPS][i];
        gp->bid.source = gp->ask.source = prices;
        for (bidask=0; bidask<2; bidask++)
        {
            price = volume = 0.;
            dir = (bidask == 0) ? 1 : -1;
            if ( bidask == 0 && i >= n )
                continue;
            else if ( bidask == 1 && i >= m )
                continue;
            if ( strcmp(prices->exchange,"bter") == 0 && dir < 0 )
                slot = ((bidask==0?n:m) - 1) - i;
            else slot = i;
            timestamp = 0;
            item = jitem(bidask==0?bids:asks,slot);
            if ( pricefield != 0 && volfield != 0 )
                price = jdouble(item,pricefield), volume = jdouble(item,volfield);
            else if ( is_cJSON_Array(item) != 0 && (numitems= cJSON_GetArraySize(item)) != 0 ) // big assumptions about order within nested array!
                price = jdouble(jitem(item,0),0), volume = jdouble(jitem(item,1),0);
            else continue;
            if ( price > SMALLVAL && volume > SMALLVAL )
            {
                if ( bidask == 0 )
                    gp->bid.price = price, gp->bid.vol = volume, gp->bid.source = prices, gp->bid.timestamp = OB.timestamp, gp->bid.ind = (OB.numbids++ << 1);
                else gp->ask.price = price, gp->ask.vol = volume, gp->ask.source = prices, gp->ask.timestamp = OB.timestamp, gp->ask.ind = (OB.numasks++ << 1) | 1;
                if ( i == 0 )
                {
                    if ( bidask == 0 )
                        prices->lastbid = price;
                    else prices->lastask = price;
                    if ( hbla == 0. )
                        hbla = price;
                    else hbla = 0.5 * (hbla + price);
                }
                if ( Debuglevel > 2 || prices->basketsize > 0 )//|| strcmp("nxtae",prices->exchange) == 0 )
                    printf("%d,%d: %-8s %s %5s/%-5s %13.8f vol %13.8f | invert %13.8f vol %13.8f | timestamp.%u\n",i,slot,prices->exchange,dir>0?"bid":"ask",prices->base,prices->rel,price,volume,1./price,volume*price,timestamp);
            }
        }
    }
    if ( hbla != 0. )
        *hblap = hbla;
    prices->O2 = prices->O;
    //prices->O = OB;
    for (i=0; i<MAX_GROUPS; i++)
        memcpy(prices->O.book[i],prices->O.book[i+1],sizeof(prices->O.book[i]));
    memcpy(prices->O.book[MAX_GROUPS],OB.book[MAX_GROUPS],sizeof(OB.book[MAX_GROUPS]));
    prices->O.numbids = OB.numbids, prices->O.numasks = OB.numasks, prices->O.timestamp = OB.timestamp;
    prices777_jsonstrs(prices,&OB);
}

double prices777_json_orderbook(char *exchangestr,struct prices777 *prices,int32_t maxdepth,cJSON *json,char *resultfield,char *bidfield,char *askfield,char *pricefield,char *volfield)
{
    cJSON *obj = 0,*bidobj,*askobj; double hbla = 0.; int32_t numasks,numbids;
    if ( resultfield == 0 )
        obj = json;
    if ( maxdepth == 0 )
        maxdepth = MAX_DEPTH;
    if ( resultfield == 0 || (obj= jobj(json,resultfield)) != 0 )
    {
        if ( (bidobj= jarray(&numbids,obj,bidfield)) != 0 && (askobj= jarray(&numasks,obj,askfield)) != 0 )
            prices777_json_quotes(&hbla,prices,bidobj,askobj,maxdepth,pricefield,volfield,0);
    }
    return(hbla);
}

void prices777_hbla(int32_t *lowaski,int32_t *highbidi,double *highbid,double *bidvol,double *lowask,double *askvol,double groupwt,int32_t i,double price,double vol)
{
    //printf("hbla.(%f %f) ",price,vol);
    if ( groupwt > SMALLVAL && (*lowask == 0. || price < *lowask) )
        *lowask = price, *askvol = vol, *lowaski = i;
    else if ( groupwt < -SMALLVAL && (*highbid == 0. || price > *highbid) )
        *highbid = price, *bidvol = vol, *highbidi = i;
}

int32_t prices777_groupbidasks(struct prices777_orderentry *gp,double groupwt,double minvol,struct prices777_basket *group,int32_t groupsize)
{
    int32_t i,highbidi,lowaski; double highbid,lowask,bidvol,askvol,vol,price,polarity; struct prices777 *feature;
    memset(gp,0,sizeof(*gp));
    highbidi = lowaski = -1;
    for (bidvol=askvol=highbid=lowask=i=0; i<groupsize; i++)
    {
        if ( (feature= group[i].prices) != 0 )
        {
            if ( i > 0 && strcmp(feature->base,group[0].rel) == 0 && strcmp(feature->rel,group[0].base) == 0 )
                polarity = -1.;
            else polarity = 1.;
            if ( group[i].bidi < feature->O.numbids && (vol= feature->O.book[MAX_GROUPS][group[i].bidi].bid.vol) > minvol && (price= feature->O.book[MAX_GROUPS][group[i].bidi].bid.price) > SMALLVAL )
            {
                if ( polarity < 0. )
                    vol *= price, price = (1. / price);
                prices777_hbla(&lowaski,&highbidi,&highbid,&bidvol,&lowask,&askvol,-polarity * groupwt,i,price,vol);
            }
            if ( group[i].aski < feature->O.numasks && (vol= feature->O.book[MAX_GROUPS][group[i].aski].ask.vol) > minvol && (price= feature->O.book[MAX_GROUPS][group[i].aski].ask.price) > SMALLVAL )
            {
                if ( polarity < 0. )
                    vol *= price, price = (1. / price);
                prices777_hbla(&lowaski,&highbidi,&highbid,&bidvol,&lowask,&askvol,polarity * groupwt,i,price,vol);
            }
        } else printf("null feature.%p\n",feature);
    }
    //printf("groupsize.%d highbidi.%d lowaski.%d\n",groupsize,highbidi,lowaski);
    gp->bid.price = highbid, gp->bid.vol = bidvol, gp->ask.price = lowask, gp->ask.vol = askvol;
    if ( highbidi >= 0 )
    {
        //printf("%s.(%d %d) groupsize.%d highbid %.8f vol %f\n",group[highbidi].prices->exchange,group[highbidi].bidi,group[highbidi].aski,groupsize,highbid,bidvol);
        gp->bid.ind = group[highbidi].bidi++, gp->bid.ind <<= 1;
        gp->bid.source = group[highbidi].prices;
        gp->bid.wt = group[highbidi].wt;
        gp->bid.timestamp = group[highbidi].prices->O.timestamp;
    } //else printf("warning: no highbidi? [%f %f %f %f]\n",highbid,bidvol,lowask,askvol);
    if ( lowaski >= 0 )
    {
        //printf("%s.(%d %d) groupsize.%d lowask %.8f vol %f\n",group[lowaski].prices->exchange,group[lowaski].bidi,group[lowaski].aski,groupsize,lowask,askvol);
        gp->ask.ind = group[lowaski].aski++, gp->ask.ind <<= 1, gp->ask.ind |= 1;
        gp->ask.source = group[lowaski].prices;
        gp->ask.wt = group[highbidi].wt;
        gp->ask.timestamp = group[lowaski].prices->O.timestamp;
    } //else printf("warning: no lowaski? [%f %f %f %f]\n",highbid,bidvol,lowask,askvol);
    if ( gp->bid.price > SMALLVAL && gp->ask.price > SMALLVAL )
        return(0);
    return(-1);
}

double prices777_basket(struct prices777 *prices,int32_t maxdepth)
{
    int32_t i,j,groupsize,slot; double a,av,b,bv,gap,bid,ask,minvol,bidvol,askvol,hbla = 0.; struct prices777_basketinfo OB;
    uint32_t timestamp; struct prices777 *feature; struct prices777_orderentry *gp;
    timestamp = (uint32_t)time(NULL);
    memset(&OB,0,sizeof(OB));
    OB.timestamp = timestamp;
    for (i=0; i<prices->basketsize; i++)
    {
        if ( (feature= prices->basket[i].prices) != 0 )
        {
            if ( (gap= (prices->lastupdate - feature->lastupdate)) < 0 )
            {
                printf("you can ignore this harmless warning about unexpected time traveling feature %f vs %f or laggy feature\n",prices->lastupdate,feature->lastupdate);
                return(0.);
            }
        }
        else
        {
            printf("unexpected null basket item %s[%d]\n",prices->contract,i);
            return(0.);
        }
        prices->basket[i].aski = prices->basket[i].bidi = 0;
    }
    //printf("%s basketsize.%d numgroups.%d maxdepth.%d group0size.%d\n",prices->contract,prices->basketsize,prices->numgroups,maxdepth,prices->basket[0].groupsize);
    for (slot=0; slot<maxdepth; slot++)
    {
        groupsize = prices->basket[0].groupsize;
        minvol = (1. / SATOSHIDEN);
        bid = ask = 1.; bidvol = askvol = 0.;
        for (j=i=0; j<prices->numgroups; j++,i+=groupsize)
        {
            groupsize = prices->basket[i].groupsize;
            gp = &OB.book[j][slot];
            //printf("j%d slot.%d %s numgroups.%d groupsize.%d %p\n",j,slot,prices->contract,prices->numgroups,groupsize,&prices->basket[0].groupsize);
            if ( prices777_groupbidasks(gp,prices->groupwts[j],minvol,&prices->basket[i],groupsize) != 0 )
                break;
//printf("[%f %f %f %f] bid %f ask %f \n",gp->ask,gp->bid,gp->askvol,gp->bidvol,bid,ask);
            if ( bid > SMALLVAL && (b= gp->bid.price) > SMALLVAL && (bv= gp->bid.vol) > SMALLVAL )
            {
                if ( prices->groupwts[j] < 0 )
                    bv *= (b / bid), bid /= b;
                else bid *= b;
                if ( bidvol == 0. )
                    bidvol = bv;
                else
                {
                    if ( prices->groupwts[j] < 0 )
                        bv *= b;
                    else bv *= b, bv /= bid;
                    if ( bv < bidvol )
                        bidvol = bv;
                }
            } else bid = bidvol = 0.;
            if ( ask > SMALLVAL && (a= gp->ask.price) > SMALLVAL && (av= gp->ask.vol) > SMALLVAL )
            {
                if ( prices->groupwts[j] < 0 )
                    av *= (a / ask), ask /= a;
                else ask *= a;
                if ( askvol == 0. )
                    askvol = av;
                else
                {
                    if ( prices->groupwts[j] < 0 )
                        av *= a;
                    else av *= a, av /= ask;
                    if ( av < askvol )
                        askvol = av;
                }
            } else ask = askvol = 0.;
            if ( Debuglevel > 2 )
                printf("%s wt %2.0f j.%d: b %.8f %12.6f a %.8f %12.6f, bid %.8f %12.6f ask %.8f %12.6f inv %f %f\n",prices->contract,prices->groupwts[j],j,b,bv,a,av,bid,bidvol,ask,askvol,bv,av);
        }
        gp = &OB.book[MAX_GROUPS][slot];
        if ( bid > SMALLVAL && bidvol > SMALLVAL )
        {
            if ( slot == 0 )
                prices->lastbid = bid;
            gp->bid.timestamp = OB.timestamp, gp->bid.price = bid, gp->bid.vol = bidvol, gp->bid.ind = OB.numbids++;
            gp->bid.source = prices;
        }
        if ( ask > SMALLVAL && askvol > SMALLVAL )
        {
            if ( slot == 0 )
                prices->lastask = ask;
            gp->ask.timestamp = OB.timestamp, gp->ask.price = ask, gp->ask.vol = askvol, gp->ask.ind = OB.numasks++;
            gp->ask.source = prices;
        }
        //printf("%s slot.%d (%f %f %f %f) (%d %d)\n",prices->contract,slot,gp->bid,gp->bidvol,gp->ask,gp->askvol,OB.numbids,OB.numasks);
    }
    if ( slot > 0 )
    {
        prices->O2 = prices->O;
        prices->O = OB;
        prices777_jsonstrs(prices,&OB);
    }
    if ( prices->lastbid > SMALLVAL && prices->lastask > SMALLVAL )
        hbla = 0.5 * (prices->lastbid + prices->lastask);
    return(hbla);
}

struct prices777 *prices777_addbundle(int32_t *validp,int32_t loadprices,struct prices777 *prices,char *exchangestr,uint64_t baseid,uint64_t relid)
{
    int32_t j; struct prices777 *ptr; struct exchange_info *exchange;
    *validp = -1;
    if ( prices != 0 )
    {
        exchangestr = prices->exchange;
        baseid = prices->baseid, relid = prices->relid;
    }
    for (j=0; j<BUNDLE.num; j++)
    {
        if ( (ptr= BUNDLE.ptrs[j]) != 0 && ((ptr->baseid == baseid && ptr->relid == relid) || (ptr->relid == baseid && ptr->baseid == relid)) && strcmp(ptr->exchange,exchangestr) == 0 )
            return(ptr);
    }
    if ( j == BUNDLE.num )
    {
        if ( prices != 0 )
        {
            exchange = &Exchanges[prices->exchangeid];
            if ( loadprices != 0 && exchange->updatefunc != 0 )
            {
                portable_mutex_lock(&exchange->mutex);
                (exchange->updatefunc)(prices,MAX_DEPTH);
                portable_mutex_unlock(&exchange->mutex);
            }
            printf("total polling.%d added.(%s)\n",BUNDLE.num,prices->contract);
            if ( exchange->polling == 0 )
            {
                printf("First pair for (%s), start polling]\n",exchange_str(prices->exchangeid));
                exchange->polling = 1;
                portable_thread_create((void *)prices777_exchangeloop,&Exchanges[prices->exchangeid]);
            }
            BUNDLE.ptrs[BUNDLE.num++] = prices;
            printf("prices777_addbundle.(%s) (%s/%s).%s %llu %llu\n",prices->contract,prices->base,prices->rel,prices->exchange,(long long)prices->baseid,(long long)prices->rel);
        }
        *validp = BUNDLE.num;
        return(0);
    } else printf("(%llu/%llu).%s already there\n",(long long)baseid,(long long)relid,exchangestr);
    return(0);
}

struct prices777 *prices777_makebasket(char *basketstr,cJSON *_basketjson)
{
    //{"name":"NXT/BTC","base":"NXT","rel":"BTC","basket":[{"exchange":"poloniex"},{"exchange":"btc38"}]}
    int32_t i,n,keysize,groupid,valid; char refname[64],refbase[64],refrel[64],name[64],base[64],rel[64],exchangestr[64],key[8192];
    uint64_t baseid,relid,refbaseid=0,refrelid=0; double wt;
    struct prices777_basket *basket = 0; cJSON *basketjson,*array,*item; struct prices777 *prices = 0;
    if ( (basketjson= _basketjson) == 0 && (basketjson= cJSON_Parse(basketstr)) == 0 )
    {
        printf("cant parse basketstr.(%s)\n",basketstr);
        return(0);
    }
    copy_cJSON(refname,jobj(basketjson,"name"));
    copy_cJSON(refbase,jobj(basketjson,"base"));
    copy_cJSON(refrel,jobj(basketjson,"rel"));
    if ( (array= jarray(&n,basketjson,"basket")) != 0 )
    {
        basket = calloc(1,sizeof(*basket) * n);
        for (i=0; i<n; i++)
        {
            item = jitem(array,i);
            copy_cJSON(exchangestr,jobj(item,"exchange"));
            if ( exchange_find(exchangestr) == 0 )
            {
                printf("ERROR: >>>>>>>>>>>> unsupported exchange.(%s)\n",exchangestr);
                if ( basketjson != _basketjson )
                    free_json(basketjson);
                free(basket);
                return(0);
            }
            copy_cJSON(name,jobj(item,"name"));
            copy_cJSON(base,jobj(item,"base"));
            if ( base[0] == 0 )
                strcpy(base,refbase);
            copy_cJSON(rel,jobj(item,"rel"));
            if ( rel[0] == 0 )
                strcpy(rel,refrel);
            baseid = j64bits(item,"baseid");
            relid = j64bits(item,"relid");
            groupid = juint(item,"group");
            wt = jdouble(item,"wt");
            if ( wt == 0. )
                wt = 1.;
            InstantDEX_name(key,&keysize,exchangestr,name,base,&baseid,rel,&relid);
            if ( (prices= prices777_initpair(1,0,exchangestr,base,rel,0.,name,baseid,relid,n)) != 0 )
            {
                prices777_addbundle(&valid,0,prices,0,0,0);
                basket[i].prices = prices;
                basket[i].wt = wt;
                basket[i].groupid = groupid;
                strcpy(basket[i].base,base);
                strcpy(basket[i].rel,rel);
            }
            else
            {
                printf("error creating basket item.%d (%s %s).%s\n",i,base,rel,exchangestr);
                if ( basketjson != _basketjson )
                    free_json(basketjson);
                free(basket);
                return(0);
            }
        }
        printf(">>>>> (%s/%s).%s %llu %llu\n",refbase,refrel,"basket",(long long)refbaseid,(long long)refrelid);
        InstantDEX_name(key,&keysize,"basket",refname,refbase,&refbaseid,refrel,&refrelid);
        prices777_addbundle(&valid,0,0,"basket",refbaseid,refrelid);
        printf("<<<<< valid.%d (%s/%s).%s %llu %llu\n",valid,refbase,refrel,"basket",(long long)refbaseid,(long long)refrelid);
        if ( valid >= 0 )
        {
            BUNDLE.ptrs[BUNDLE.num++] = prices = prices777_createbasket(refname,refbase,refrel,refbaseid,refrelid,basket,n);
            //prices->lastprice = prices777_basket(prices,MAX_DEPTH);
            printf("C total polling.%d added.(%s/%s).%s updating basket lastprice %f changed.%p %d groupsize.%d numgroups.%d %p\n",BUNDLE.num,prices->base,prices->rel,prices->exchange,prices->lastprice,&prices->changed,prices->changed,prices->basket[0].groupsize,prices->numgroups,&prices->basket[0].groupsize);
        } else prices = 0;
        if ( basketjson != _basketjson )
            free_json(basketjson);
        free(basket);
    }
    return(prices);
}

cJSON *inner_json(double price,double vol,uint32_t timestamp,uint64_t quoteid,uint64_t nxt64bits,uint64_t qty,uint64_t pqt,uint64_t baseamount,uint64_t relamount)
{
    cJSON *inner = cJSON_CreateArray();
    char numstr[64];
    sprintf(numstr,"%.8f",price), cJSON_AddItemToArray(inner,cJSON_CreateString(numstr));
    sprintf(numstr,"%.8f",vol), cJSON_AddItemToArray(inner,cJSON_CreateString(numstr));
    cJSON_AddItemToArray(inner,cJSON_CreateNumber(timestamp));
    sprintf(numstr,"%llu",(long long)quoteid), cJSON_AddItemToArray(inner,cJSON_CreateString(numstr));
    sprintf(numstr,"%llu",(long long)nxt64bits), cJSON_AddItemToArray(inner,cJSON_CreateString(numstr));
    sprintf(numstr,"%llu",(long long)qty), cJSON_AddItemToArray(inner,cJSON_CreateString(numstr));
    sprintf(numstr,"%llu",(long long)pqt), cJSON_AddItemToArray(inner,cJSON_CreateString(numstr));
    sprintf(numstr,"%llu",(long long)baseamount), cJSON_AddItemToArray(inner,cJSON_CreateString(numstr));
    sprintf(numstr,"%llu",(long long)relamount), cJSON_AddItemToArray(inner,cJSON_CreateString(numstr));
   // printf("(%s) ",jprint(inner,0));
    return(inner);
}

double prices777_NXT(struct prices777 *prices,int32_t maxdepth)
{
    uint32_t timestamp; int32_t flip,i,n; uint64_t baseamount,relamount,qty,pqt; char url[1024],*str,*cmd,*field;
    cJSON *json,*bids,*asks,*srcobj,*item,*array; double price,vol,hbla = 0.;
    if ( NXT_ASSETID != stringbits("NXT") || (strcmp(prices->rel,"NXT") != 0 && strcmp(prices->rel,"5527630") != 0) )
        printf("NXT_ASSETID.%llu != %llu stringbits rel.%s\n",(long long)NXT_ASSETID,(long long)stringbits("NXT"),prices->rel), getchar();
    bids = cJSON_CreateArray(), asks = cJSON_CreateArray();
    for (flip=0; flip<2; flip++)
    {
        /// xxx add MScoin
        if ( flip == 0 )
            cmd = "getBidOrders", field = "bidOrders", array = bids;
        else cmd = "getAskOrders", field = "askOrders", array = asks;
        sprintf(url,"requestType=%s&asset=%llu&limit=%d",cmd,(long long)prices->baseid,maxdepth);
        if ( (str= issue_NXTPOST(url)) != 0 )
        {
            //printf("{%s}\n",str);
            if ( (json= cJSON_Parse(str)) != 0 )
            {
                if ( (srcobj= jarray(&n,json,field)) != 0 )
                {
                    for (i=0; i<n && i<maxdepth; i++)
                    {
                        /*
                         "quantityQNT": "79",
                         "priceNQT": "13499000000",
                         "transactionHeight": 480173,
                         "accountRS": "NXT-FJQN-8QL2-BMY3-64VLK",
                         "transactionIndex": 1,
                         "asset": "15344649963748848799",
                         "type": "ask",
                         "account": "5245394173527769812",
                         "order": "17926122097022414596",
                         "height": 480173
                         */
                        item = cJSON_GetArrayItem(srcobj,i);
                        qty = j64bits(item,"quantityQNT"), pqt = j64bits(item,"priceNQT");
                        baseamount = (qty * prices->ap_mult), relamount = (qty * pqt);
                        price = prices777_price_volume(&vol,baseamount,relamount);
                        if ( i == 0  )
                        {
                            hbla = (hbla == 0.) ? price : 0.5 * (price + hbla);
                            if ( flip == 0 )
                                prices->lastbid = price;
                            else prices->lastask = price;
                        }
                        //printf("(%llu %llu) %f %f mult.%llu qty.%llu pqt.%llu baseamount.%lld relamount.%lld\n",(long long)prices->baseid,(long long)prices->relid,price,vol,(long long)prices->ap_mult,(long long)qty,(long long)pqt,(long long)baseamount,(long long)relamount);
                        timestamp = get_blockutime(juint(item,"height"));
                        item = inner_json(price,vol,timestamp,j64bits(item,"order"),j64bits(item,"account"),qty,pqt,baseamount,relamount);
                        cJSON_AddItemToArray(array,item);
                    }
                }
            }
            free(str);
        } else printf("cant get.(%s)\n",url);
    }
    json = cJSON_CreateObject();
    cJSON_AddItemToObject(json,"bids",bids);
    cJSON_AddItemToObject(json,"asks",asks);
    if ( Debuglevel > 2 )
        printf("NXTAE.(%s)\n",jprint(json,0));
    prices777_json_orderbook("nxtae",prices,maxdepth,json,0,"bids","asks",0,0);
    free_json(json);
    return(hbla);
}

double prices777_InstantDEX(struct prices777 *prices,int32_t maxdepth)
{
    double hbla = 0.;
    return(hbla);
}

/*int32_t update_iQ_flags(struct NXT_tx *txptrs[],int32_t maxtx,uint64_t refassetid)
{
    uint64_t quoteid,assetid,amount,qty,priceNQT; cJSON *json,*array,*txobj,*attachment,*msgobj,*commentobj;
    char cmd[1024],txidstr[MAX_JSON_FIELD],account[MAX_JSON_FIELD],comment[MAX_JSON_FIELD],*jsonstr; int32_t i,n,numbooks=0,type,subtype,m = 0;
    void **obooks = 0;
    txptrs[0] = 0;
    sprintf(cmd,"requestType=getUnconfirmedTransactions");
    if ( (jsonstr= issue_NXTPOST(cmd)) != 0 )
    {
        if ( (json= cJSON_Parse(jsonstr)) != 0 )
        {
            if ( (array= cJSON_GetObjectItem(json,"unconfirmedTransactions")) != 0 && is_cJSON_Array(array) != 0 && (n= cJSON_GetArraySize(array)) > 0 )
            {
                for (i=0; i<n; i++)
                {
                    txobj = cJSON_GetArrayItem(array,i);
                    copy_cJSON(txidstr,cJSON_GetObjectItem(txobj,"transaction"));
                    copy_cJSON(account,cJSON_GetObjectItem(txobj,"account"));
                    if ( account[0] == 0 )
                        copy_cJSON(account,cJSON_GetObjectItem(txobj,"sender"));
                    qty = amount = assetid = quoteid = 0;
                    amount = get_API_nxt64bits(cJSON_GetObjectItem(txobj,"amountNQT"));
                    type = (int32_t)get_API_int(cJSON_GetObjectItem(txobj,"type"),-1);
                    subtype = (int32_t)get_API_int(cJSON_GetObjectItem(txobj,"subtype"),-1);
                    if ( (attachment= cJSON_GetObjectItem(txobj,"attachment")) != 0 )
                    {
                        assetid = get_API_nxt64bits(cJSON_GetObjectItem(attachment,"asset"));
                        comment[0] = 0;
                        if ( (msgobj= cJSON_GetObjectItem(attachment,"message")) != 0 )
                        {
                            qty = get_API_nxt64bits(cJSON_GetObjectItem(attachment,"quantityQNT"));
                            priceNQT = get_API_nxt64bits(cJSON_GetObjectItem(attachment,"priceNQT"));
                            copy_cJSON(comment,msgobj);
                            if ( comment[0] != 0 )
                            {
                                unstringify(comment);
                                if ( (commentobj= cJSON_Parse(comment)) != 0 )
                                {
                                    quoteid = get_API_nxt64bits(cJSON_GetObjectItem(commentobj,"quoteid"));
                                    if ( Debuglevel > 2 )
                                        printf("acct.(%s) pending quoteid.%llu asset.%llu qty.%llu %.8f amount %.8f %d:%d tx.%s\n",account,(long long)quoteid,(long long)assetid,(long long)qty,dstr(priceNQT),dstr(amount),type,subtype,txidstr);
                                    if ( quoteid != 0 )
                                        match_unconfirmed(obooks,numbooks,account,quoteid);
                                    free_json(commentobj);
                                }
                            }
                        }
                        if ( txptrs != 0 && m < maxtx && (refassetid == 0 || refassetid == assetid) )
                        {
                            txptrs[m] = set_NXT_tx(txobj);
                            txptrs[m]->timestamp = calc_expiration(txptrs[m]);
                            txptrs[m]->quoteid = quoteid;
                            strcpy(txptrs[m]->comment,comment);
                            m++;
                            //printf("m.%d: assetid.%llu type.%d subtype.%d price.%llu qty.%llu time.%u vs %ld deadline.%d\n",m,(long long)txptrs[m-1]->assetidbits,txptrs[m-1]->type,txptrs[m-1]->subtype,(long long)txptrs[m-1]->priceNQT,(long long)txptrs[m-1]->U.quantityQNT,txptrs[m-1]->timestamp,time(NULL),txptrs[m-1]->deadline);
                        }
                    }
                }
            } free_json(json);
        } free(jsonstr);
    } free(obooks);
    txptrs[m] = 0;
    return(m);
}*/


double prices777_unconfNXT(struct prices777 *prices,int32_t maxdepth)
{
    double hbla = 0.;
    return(hbla);
}

#endif

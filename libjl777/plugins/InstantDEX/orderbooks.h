//
//  orderbooks.h
//
//  Created by jl777 on 7/9/14.
//  Copyright (c) 2014 jl777. All rights reserved.
//

#ifndef xcode_orderbooks_h
#define xcode_orderbooks_h

struct prices777 *prices777_find(int32_t *invertedp,uint64_t baseid,uint64_t relid,char *exchange)
{
    int32_t i; struct prices777 *prices;
    *invertedp = 0;
    for (i=0; i<BUNDLE.num; i++)
    {
        if ( (prices= BUNDLE.ptrs[i]) != 0 && strcmp(prices->exchange,exchange) == 0 )
        {
            if ( prices777_equiv(prices->baseid) == prices777_equiv(baseid) && prices777_equiv(prices->relid) == prices777_equiv(relid) )
                return(prices);
            else if ( prices777_equiv(prices->relid) == prices777_equiv(baseid) && prices777_equiv(prices->baseid) == prices777_equiv(relid) )
            {
                *invertedp = 1;
                return(prices);
            }
        }
    }
    return(0);
}

struct prices777 *prices777_createbasket(int32_t addbasket,char *name,char *base,char *rel,uint64_t baseid,uint64_t relid,struct prices777_basket *basket,int32_t n,char *typestr)
{
    int32_t i,j,m,iter,max = 0; double firstwt,wtsum; struct prices777 *prices,*feature;
    prices = prices777_initpair(1,0,typestr,base,rel,0.,name,baseid,relid,n);
    for (iter=0; iter<2; iter++)
    {
        for (i=0; i<n; i++)
        {
            feature = basket[i].prices;
            if ( addbasket*iter != 0 )
            {
                feature->dependents = realloc(feature->dependents,sizeof(*feature->dependents) * (feature->numdependents + 1));
                feature->dependents[feature->numdependents++] = &prices->changed;
            }
            if ( basket[i].groupid > max )
                max = basket[i].groupid;
            if ( fabs(basket[i].wt) < SMALLVAL )
            {
                printf("all basket features must have nonzero wt\n");
                free(prices);
                return(0);
            }
        }
        if ( (max+1) > MAX_GROUPS )
        {
            printf("baskets limited to %d, %d is too many for %s.(%s/%s)\n",MAX_GROUPS,n,name,base,rel);
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

void _prices777_item(cJSON *item,int32_t group,struct prices777 *prices,int32_t bidask,double price,double volume,uint64_t orderid)
{
    jaddnum(item,"group",group);
    jaddstr(item,"trade",bidask == 0 ? "sell":"buy");
    jaddstr(item,"exchange",prices->exchange);
    if ( strcmp(prices->exchange,"nxtae") == 0 || strcmp(prices->exchange,"unconf") == 0 )
        jadd64bits(item,prices->type == 2 ? "asset" : "currency",prices->baseid);
    else jaddstr(item,"name",prices->contract);
    jaddstr(item,"base",prices->base), jaddstr(item,"rel",prices->rel);
    if ( orderid != 0 )
        jadd64bits(item,"orderid",orderid);
    jaddnum(item,"orderprice",price);
    jaddnum(item,"ordervolume",volume);
}

cJSON *prices777_item(int32_t rootbidask,struct prices777 *prices,int32_t group,int32_t bidask,double origprice,double origvolume,double rootwt,double groupwt,double wt,uint64_t orderid)
{
    cJSON *item; double price,volume;
    item = cJSON_CreateObject();
    jaddstr(item,"basket",rootbidask == 0 ? "bid":"ask");
    //jaddnum(item,"rootwt",rootwt);
    //jaddnum(item,"groupwt",groupwt);
    //jaddnum(item,"wt",wt);
    if ( rootwt*groupwt < 0 )
    {
        volume = origprice * origvolume;
        price = 1./origprice;
    } else price = origprice, volume = origvolume;
    jaddnum(item,"price",price);
    jaddnum(item,"volume",volume);
    if ( groupwt*wt < 0 )
    {
        volume = origprice * origvolume;
        price = 1./origprice;
    } else price = origprice, volume = origvolume;
    _prices777_item(item,group,prices,bidask,price,volume,orderid);
    return(item);
}

cJSON *prices777_tradeitem(int32_t rootbidask,struct prices777 *prices,int32_t group,int32_t bidask,int32_t slot,uint32_t timestamp,double price,double volume,double rootwt,double groupwt,double wt,uint64_t orderid)
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
    return(prices777_item(rootbidask,prices,group,bidask,price,volume,rootwt,groupwt,wt,orderid));
}

cJSON *prices777_tradesequence(struct prices777 *prices,int32_t bidask,struct prices777_order *orders[],double rootwt,double groupwt,double wt,int32_t refgroup)
{
    int32_t i,j,srcslot,srcbidask,err = 0; cJSON *array; struct prices777_order *suborders[MAX_GROUPS];
    struct prices777_order *order; struct prices777 *src;
    array = cJSON_CreateArray();
    for (i=0; i<prices->numgroups; i++)
    {
        order = orders[i];
        groupwt = prices->groupwts[i];
        memset(suborders,0,sizeof(suborders));
        srcbidask = (order->slot_ba & 1); srcslot = order->slot_ba >> 1;
        if ( (src= order->source) != 0 )
        {
            if ( src->basketsize == 0 )
                jaddi(array,prices777_tradeitem(bidask,src,refgroup*10+i,srcbidask,srcslot,order->timestamp,order->price,order->ratio*order->vol,rootwt,groupwt,order->wt,order->id));
            else if ( src->O.timestamp == order->timestamp )
            {
                for (j=0; j<src->numgroups; j++)
                    suborders[j] = (srcbidask == 0) ? &src->O.book[j][srcslot].bid : &src->O.book[j][srcslot].ask;
                jaddi(array,prices777_tradesequence(src,bidask,suborders,rootwt,groupwt,order->wt,refgroup*10 + i));
            }
            else if ( src->O2.timestamp == order->timestamp )
            {
                for (j=0; j<src->numgroups; j++)
                    suborders[j] = (srcbidask == 0) ? &src->O2.book[j][srcslot].bid : &src->O2.book[j][srcslot].ask;
                jaddi(array,prices777_tradesequence(src,bidask,suborders,rootwt,groupwt,order->wt,refgroup*10 + i));
            }
            else err =  1;
        }
        if ( src == 0 || err != 0 )
        {
            //jaddi(array,prices777_item(prices,bidask,price,volume,wt,orderid));
            //printf("prices777_tradesequence warning cant match timestamp %u (%s %s/%s)\n",order->timestamp,prices->contract,prices->base,prices->rel);
        }
    }
    return(array);
}

void prices777_orderbook_item(struct prices777 *prices,int32_t bidask,struct prices777_order *suborders[],cJSON *array,int32_t invert,int32_t allflag,double origprice,double origvolume,uint64_t orderid)
{
    cJSON *item,*obj,*tarray; double price,volume;
    item = cJSON_CreateObject();
    if ( invert != 0 )
        volume = (origvolume * origprice), price = 1./origprice;
    else price = origprice, volume = origvolume;
    jaddstr(item,"plugin","InstantDEX"), jaddstr(item,"method","tradesequence");
    jaddnum(item,"price",price), jaddnum(item,"volume",volume);
    if ( allflag != 0 )
    {
        if ( prices->basketsize == 0 )
        {
            tarray = cJSON_CreateArray();
            obj = cJSON_CreateObject();
            _prices777_item(obj,0,prices,bidask,origprice,origvolume,orderid);
            jaddi(tarray,obj);
        } else tarray = prices777_tradesequence(prices,bidask,suborders,invert!=0?-1:1.,1.,1.,0);
        jadd(item,"trades",tarray);
    }
    jaddi(array,item);
}

char *prices777_orderbook_jsonstr(int32_t invert,uint64_t nxt64bits,struct prices777 *prices,struct prices777_basketinfo *OB,int32_t maxdepth,int32_t allflag)
{
    struct prices777_orderentry *gp; struct prices777_order *suborders[MAX_GROUPS]; cJSON *json,*bids,*asks;
    int32_t i,slot; char baserel[64],assetA[64],assetB[64],NXTaddr[64];
    if ( invert == 0 )
        sprintf(baserel,"%s/%s",prices->base,prices->rel);
    else sprintf(baserel,"%s/%s",prices->rel,prices->base);
    if ( Debuglevel > 2 )
        printf("ORDERBOOK %s/%s iQsize.%ld numbids.%d numasks.%d maxdepth.%d (%llu %llu)\n",prices->base,prices->rel,sizeof(struct InstantDEX_quote),OB->numbids,OB->numasks,maxdepth,(long long)prices->baseid,(long long)prices->relid);
    json = cJSON_CreateObject(), bids = cJSON_CreateArray(), asks = cJSON_CreateArray();
    gp = &OB->book[MAX_GROUPS][0];
    memset(suborders,0,sizeof(suborders));
    for (slot=0; slot<OB->numbids && slot<OB->numasks && slot<maxdepth; slot++,gp++)
    {
        if ( slot < OB->numbids )
        {
            for (i=0; i<prices->numgroups; i++)
                suborders[i] = &OB->book[i][slot].bid;
            prices777_orderbook_item(prices,0,suborders,(invert==0) ? bids : asks,invert,allflag,gp->bid.price,gp->bid.vol,gp->bid.id);
        }
        if ( slot < OB->numasks )
        {
            for (i=0; i<prices->numgroups; i++)
                suborders[i] = &OB->book[i][slot].ask;
            prices777_orderbook_item(prices,1,suborders,(invert==0) ? asks : bids,invert,allflag,gp->ask.price,gp->ask.vol,gp->ask.id);
        }
    }
    expand_nxt64bits(NXTaddr,nxt64bits);
    expand_nxt64bits(assetA,invert==0 ? prices->baseid : prices->relid);
    expand_nxt64bits(assetB,invert!=0 ? prices->baseid : prices->relid);
    cJSON_AddItemToObject(json,"inverted",cJSON_CreateNumber(invert));
    cJSON_AddItemToObject(json,"contract",cJSON_CreateString(prices->contract));
    cJSON_AddItemToObject(json,"baseid",cJSON_CreateString(assetA));
    cJSON_AddItemToObject(json,"relid",cJSON_CreateString(assetB));
    cJSON_AddItemToObject(json,"bids",bids);
    cJSON_AddItemToObject(json,"asks",asks);
    if ( invert == 0 )
    {
        cJSON_AddItemToObject(json,"numbids",cJSON_CreateNumber(OB->numbids));
        cJSON_AddItemToObject(json,"numasks",cJSON_CreateNumber(OB->numasks));
        cJSON_AddItemToObject(json,"lastbid",cJSON_CreateNumber(prices->lastbid));
        cJSON_AddItemToObject(json,"lastask",cJSON_CreateNumber(prices->lastask));
    }
    else
    {
        cJSON_AddItemToObject(json,"numbids",cJSON_CreateNumber(OB->numasks));
        cJSON_AddItemToObject(json,"numasks",cJSON_CreateNumber(OB->numbids));
        if ( prices->lastask != 0 )
            cJSON_AddItemToObject(json,"lastbid",cJSON_CreateNumber(1. / prices->lastask));
        if ( prices->lastbid != 0 )
            cJSON_AddItemToObject(json,"lastask",cJSON_CreateNumber(1. / prices->lastbid));
    }
    cJSON_AddItemToObject(json,"NXT",cJSON_CreateString(NXTaddr));
    cJSON_AddItemToObject(json,"timestamp",cJSON_CreateNumber(time(NULL)));
    cJSON_AddItemToObject(json,"maxdepth",cJSON_CreateNumber(maxdepth));
    return(jprint(json,1));
}

void prices777_jsonstrs(struct prices777 *prices,struct prices777_basketinfo *OB)
{
    int32_t allflag; char *strs[4];
    if ( OB->numbids == 0 && OB->numasks == 0 )
    {
        printf("warning: updating null orderbook ignored for %s (%s/%s)\n",prices->contract,prices->base,prices->rel);
        return;
    }
    for (allflag=0; allflag<4; allflag++)
    {
        strs[allflag] = prices777_orderbook_jsonstr(allflag/2,SUPERNET.my64bits,prices,OB,MAX_DEPTH,allflag%2);
        if ( Debuglevel > 2 )
            printf("strs[%d].(%s) prices.%p\n",allflag,strs[allflag],prices);
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
    cJSON *item; int32_t i,slot,n=0,m=0,dir,bidask,numitems; uint64_t orderid; uint32_t timestamp; double price,volume,hbla = 0.;
    struct prices777_basketinfo OB; struct prices777_orderentry *gp; struct prices777_order *order;
    memset(&OB,0,sizeof(OB));
    if ( reftimestamp == 0 )
        reftimestamp = (uint32_t)time(NULL);
    OB.timestamp = reftimestamp;
    if ( bids != 0 )
    {
        n = cJSON_GetArraySize(bids);
        if ( maxdepth != 0 && n > maxdepth )
            n = maxdepth;
    }
    if ( asks != 0 )
    {
        m = cJSON_GetArraySize(asks);
        if ( maxdepth != 0 && m > maxdepth )
            m = maxdepth;
    }
    for (i=0; i<n||i<m; i++)
    {
        gp = &OB.book[MAX_GROUPS][i];
        gp->bid.source = gp->ask.source = prices;
        for (bidask=0; bidask<2; bidask++)
        {
            price = volume = 0.;
            orderid = 0;
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
                price = jdouble(jitem(item,0),0), volume = jdouble(jitem(item,1),0), orderid = j64bits(jitem(item,2),0);
            else continue;
            if ( price > SMALLVAL && volume > SMALLVAL )
            {
                order = (bidask == 0) ? &gp->bid : &gp->ask;
                order->price = price, order->vol = volume, order->source = prices, order->timestamp = OB.timestamp, order->wt = 1, order->id = orderid;
                if ( bidask == 0 )
                    order->slot_ba = (OB.numbids++ << 1);
                else order->slot_ba = (OB.numasks++ << 1) | 1;
                if ( i == 0 )
                {
                    if ( bidask == 0 )
                        prices->lastbid = price;
                    else prices->lastask = price;
                    if ( hbla == 0. )
                        hbla = price;
                    else hbla = 0.5 * (hbla + price);
                }
                if ( Debuglevel > 2 || prices->basketsize > 0 || strcmp("unconf",prices->exchange) == 0 )
                    printf("%d,%d: %-8s %s %5s/%-5s %13.8f vol %13.8f | invert %13.8f vol %13.8f | timestamp.%u\n",OB.numbids,OB.numasks,prices->exchange,dir>0?"bid":"ask",prices->base,prices->rel,price,volume,1./price,volume*price,timestamp);
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
}

double prices777_json_orderbook(char *exchangestr,struct prices777 *prices,int32_t maxdepth,cJSON *json,char *resultfield,char *bidfield,char *askfield,char *pricefield,char *volfield)
{
    cJSON *obj = 0,*bidobj=0,*askobj=0; double hbla = 0.; int32_t numasks=0,numbids=0;
    if ( resultfield == 0 )
        obj = json;
    if ( maxdepth == 0 )
        maxdepth = MAX_DEPTH;
    if ( resultfield == 0 || (obj= jobj(json,resultfield)) != 0 )
    {
        bidobj = jarray(&numbids,obj,bidfield);
        askobj = jarray(&numasks,obj,askfield);
        if ( bidobj != 0 || askobj != 0 )
        {
            prices777_json_quotes(&hbla,prices,bidobj,askobj,maxdepth,pricefield,volfield,0);
            prices777_jsonstrs(prices,&prices->O);
        }
    }
    return(hbla);
}

void prices777_hbla(uint64_t *bidorderid,uint64_t *askorderid,int32_t *lowaski,int32_t *highbidi,double *highbid,double *bidvol,double *lowask,double *askvol,double groupwt,int32_t i,int32_t bidask,double price,double vol,uint64_t orderid)
{
    //printf("hbla.(%f %f) ",price,vol);
    if ( groupwt > SMALLVAL && (*lowask == 0. || price < *lowask) )
        *askorderid = orderid, *lowask = price, *askvol = vol, *lowaski = (i << 1) | bidask;
    else if ( groupwt < -SMALLVAL && (*highbid == 0. || price > *highbid) )
        *bidorderid = orderid, *highbid = price, *bidvol = vol, *highbidi = (i << 1) | bidask;
}

void prices777_setorder(struct prices777_order *order,struct prices777_basket *group,int32_t coordinate,uint64_t orderid)
{
    int32_t bidask; struct prices777 *prices;
    bidask = (coordinate & 1), coordinate >>= 1;
    prices = group[coordinate].prices;
    if ( bidask != 0 && group[coordinate].aski < prices->O.numasks )
        order->slot_ba = (group[coordinate].aski++ << 1) | 1;
    else if ( group[coordinate].bidi < prices->O.numbids )
        order->slot_ba = (group[coordinate].bidi++ << 1);
    else printf("illegal coordinate.%d bidask.%d when bidi.%d aski.%d numbids.%d numasks.%d\n",coordinate,bidask,group[coordinate].bidi,group[coordinate].aski,prices->O.numbids,prices->O.numasks);
    order->source = prices;
    order->wt = group[coordinate].wt;
    //printf("coordinate.%d wt %f\n",coordinate,order->wt);
    order->timestamp = prices->O.timestamp;
    order->id = orderid;
}

int32_t prices777_groupbidasks(struct prices777_orderentry *gp,double groupwt,double minvol,struct prices777_basket *group,int32_t groupsize)
{
    int32_t i,highbidi,lowaski; double highbid,lowask,bidvol,askvol,vol,price,polarity; uint64_t bidorderid,askorderid;
    struct prices777 *feature; struct prices777_order *order;
    memset(gp,0,sizeof(*gp));
    highbidi = lowaski = -1;
    for (bidvol=askvol=highbid=lowask=bidorderid=askorderid=i=0; i<groupsize; i++)
    {
        if ( (feature= group[i].prices) != 0 )
        {
            if ( i > 0 && strcmp(feature->base,group[0].rel) == 0 && strcmp(feature->rel,group[0].base) == 0 )
                polarity = -1.;
            else polarity = 1.;
            order = &feature->O.book[MAX_GROUPS][group[i].bidi].bid;
            if ( group[i].bidi < feature->O.numbids && (vol= order->vol) > minvol && (price= order->price) > SMALLVAL )
            {
                if ( polarity < 0. )
                    vol *= price, price = (1. / price);
                prices777_hbla(&bidorderid,&askorderid,&lowaski,&highbidi,&highbid,&bidvol,&lowask,&askvol,-polarity * groupwt,i,0,price,vol,order->id);
            }
            order = &feature->O.book[MAX_GROUPS][group[i].aski].ask;
            if ( group[i].aski < feature->O.numasks && (vol= order->vol) > minvol && (price= order->price) > SMALLVAL )
            {
                if ( polarity < 0. )
                    vol *= price, price = (1. / price);
                prices777_hbla(&bidorderid,&askorderid,&lowaski,&highbidi,&highbid,&bidvol,&lowask,&askvol,polarity * groupwt,i,1,price,vol,order->id);
            }
        } else printf("null feature.%p\n",feature);
    }
    //printf("groupsize.%d highbidi.%d lowaski.%d\n",groupsize,highbidi,lowaski);
    gp->bid.price = highbid, gp->bid.vol = bidvol, gp->ask.price = lowask, gp->ask.vol = askvol;
    if ( highbidi >= 0 )
        prices777_setorder(&gp->bid,group,highbidi,bidorderid);
    if ( lowaski >= 0 )
        prices777_setorder(&gp->ask,group,lowaski,askorderid);
    if ( gp->bid.price > SMALLVAL && gp->ask.price > SMALLVAL )
        return(0);
    return(-1);
}

double prices777_volcalc(double *basevols,uint64_t *baseids,uint64_t baseid,double basevol)
{
    int32_t i;
    //printf("(add %llu %f) ",(long long)baseid,basevol);
    for (i=0; i<MAX_GROUPS*2; i++)
    {
        if ( baseids[i] == baseid )
        {
            if ( basevols[i] == 0. || basevol < basevols[i] )
                basevols[i] = basevol;//, printf("set %llu <= %f ",(long long)baseid,basevol);
           // else printf("missed basevols[%d] %f, ",i,basevols[i]);
            break;
        }
    }
    return(1);
}

double prices777_volratio(double *basevols,uint64_t *baseids,uint64_t baseid,double vol)
{
    int32_t i;
    for (i=0; i<MAX_GROUPS*2; i++)
    {
        if ( baseids[i] == baseid )
        {
            if ( basevols[i] > 0. )
            {
                //printf("(vol %f vs %f) ",vol,basevols[i]);
                if ( vol > basevols[i] )
                    return(basevols[i]/vol);
                else return(1.);
            }
            printf("unexpected zero vol basevols.%d\n",i);
            return(1.);
            break;
        }
    }
    printf("unexpected cant find baseid.%llu\n",(long long)baseid);
    return(1.);
}

double prices777_basket(struct prices777 *prices,int32_t maxdepth)
{
    int32_t i,j,groupsize,slot; uint64_t baseids[MAX_GROUPS*2];
    double basevols[MAX_GROUPS*2],relvols[MAX_GROUPS*2],baseratio,relratio,a,av,b,bv,gap,bid,ask,minvol,bidvol,askvol,hbla = 0.;
    struct prices777_basketinfo OB; uint32_t timestamp; struct prices777 *feature; struct prices777_orderentry *gp;
    timestamp = (uint32_t)time(NULL);
    memset(&OB,0,sizeof(OB));
    memset(baseids,0,sizeof(baseids));
    OB.timestamp = timestamp;
    for (i=0; i<prices->basketsize; i++)
    {
        if ( (feature= prices->basket[i].prices) != 0 )
        {
            if ( (gap= (prices->lastupdate - feature->lastupdate)) < 0 )
            {
                if ( prices->lastupdate != 0 )
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
        for (j=0; j<MAX_GROUPS*2; j++)
        {
            if ( prices->basket[i].prices->baseid == baseids[j] )
                break;
            if ( baseids[j] == 0 )
            {
                baseids[j] = prices->basket[i].prices->baseid;
                break;
            }
        }
        for (j=0; j<MAX_GROUPS*2; j++)
        {
            if ( prices->basket[i].prices->relid == baseids[j] )
                break;
            if ( baseids[j] == 0 )
            {
                baseids[j] = prices->basket[i].prices->relid;
                break;
            }
        }
    }
    //printf("%s basketsize.%d numgroups.%d maxdepth.%d group0size.%d\n",prices->contract,prices->basketsize,prices->numgroups,maxdepth,prices->basket[0].groupsize);
    for (slot=0; slot<maxdepth; slot++)
    {
        memset(basevols,0,sizeof(basevols));
        memset(relvols,0,sizeof(relvols));
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
            if ( bid > SMALLVAL && (b= gp->bid.price) > SMALLVAL && (bv= gp->bid.vol) > SMALLVAL )
            {
                if ( prices->groupwts[j] < 0 )
                    bid /= b;
                else bid *= b;
                prices777_volcalc(basevols,baseids,gp->bid.source->baseid,bv);
                prices777_volcalc(basevols,baseids,gp->bid.source->relid,b*bv);
                //printf("bid %f b %f bv %f %s %s %f\n",bid,b,bv,gp->bid.source->base,gp->bid.source->rel,bv*b);
            } else bid = 0.;
            if ( ask > SMALLVAL && (a= gp->ask.price) > SMALLVAL && (av= gp->ask.vol) > SMALLVAL )
            {
                if ( prices->groupwts[j] < 0 )
                    ask /= a;
                else ask *= a;
                prices777_volcalc(relvols,baseids,gp->ask.source->baseid,av);
                prices777_volcalc(relvols,baseids,gp->ask.source->relid,a*av);
                //printf("ask %f b %f bv %f %s %s %f\n",ask,a,av,gp->ask.source->base,gp->ask.source->rel,av*a);
            } else ask = 0.;
            if ( Debuglevel > 2 )
                printf("%s wt %2.0f j.%d: b %.8f %12.6f a %.8f %12.6f, bid %.8f %12.6f ask %.8f %12.6f inv %f %f\n",prices->contract,prices->groupwts[j],j,b,bv,a,av,bid,bidvol,ask,askvol,bv,av);
        }
        for (j=0; j<prices->numgroups; j++)
        {
            gp = &OB.book[j][slot];
            if ( gp->bid.source == 0 || gp->ask.source == 0 )
            {
                //printf("null source slot.%d j.%d\n",slot,j);
                break;
            }
            baseratio = prices777_volratio(basevols,baseids,gp->bid.source->baseid,gp->bid.vol);
            relratio = prices777_volratio(basevols,baseids,gp->bid.source->relid,gp->bid.vol * gp->bid.price);
            gp->bid.ratio = (baseratio < relratio) ? baseratio : relratio;
            if ( j == 0 )
                bidvol = (gp->bid.ratio * gp->bid.vol);
            //printf("(%f %f) (%f %f) bid%d bidratio %f bidvol %f ",gp->bid.vol,baseratio,gp->bid.vol * gp->bid.price,relratio,j,gp->bid.ratio,bidvol);
            baseratio = prices777_volratio(relvols,baseids,gp->ask.source->baseid,gp->ask.vol);
            relratio = prices777_volratio(relvols,baseids,gp->ask.source->relid,gp->ask.vol * gp->ask.price);
            gp->ask.ratio = (baseratio < relratio) ? baseratio : relratio;
            if ( j == 0 )
                askvol = (gp->ask.ratio * gp->ask.vol);
            //printf("(%f %f) (%f %f) ask%d askratio %f askvol %f\n",gp->ask.vol,baseratio,gp->ask.vol * gp->ask.price,relratio,j,gp->ask.ratio,askvol);
        }
        if ( j != prices->numgroups )
            break;
        for (j=0; j<MAX_GROUPS*2; j++)
        {
            if ( baseids[j] == 0 )
                break;
            //printf("{%llu %f %f} ",(long long)baseids[j],basevols[j],relvols[j]);
        }
        //printf("basevols bidvol %f, askvol %f\n",bidvol,askvol);
        gp = &OB.book[MAX_GROUPS][slot];
        if ( bid > SMALLVAL && bidvol > SMALLVAL )
        {
            if ( slot == 0 )
                prices->lastbid = bid;
            gp->bid.timestamp = OB.timestamp, gp->bid.price = bid, gp->bid.vol = bidvol, gp->bid.slot_ba = (OB.numbids++ << 1);
            gp->bid.source = prices, gp->bid.wt = prices->groupwts[j];
        }
        if ( ask > SMALLVAL && askvol > SMALLVAL )
        {
            if ( slot == 0 )
                prices->lastask = ask;
            gp->ask.timestamp = OB.timestamp, gp->ask.price = ask, gp->ask.vol = askvol, gp->ask.slot_ba = (OB.numasks++ << 1) | 1;
            gp->ask.source = prices, gp->ask.wt = prices->groupwts[j];
        }
        //printf("%s slot.%d (%f %f %f %f) (%d %d)\n",prices->contract,slot,gp->bid.price,gp->bid.vol,gp->ask.price,gp->ask.vol,OB.numbids,OB.numasks);
    }
    fprintf(stderr,"basket.%s slot.%d numbids.%d numasks.%d\n",prices->contract,slot,prices->O.numbids,prices->O.numasks);
    if ( slot > 0 )
    {
        prices->O2 = prices->O;
        prices->O = OB;
        if ( prices->lastbid > SMALLVAL && prices->lastask > SMALLVAL )
            hbla = 0.5 * (prices->lastbid + prices->lastask);
    }
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
            printf("prices777_addbundle.(%s) (%s/%s).%s %llu %llu\n",prices->contract,prices->base,prices->rel,prices->exchange,(long long)prices->baseid,(long long)prices->relid);
        }
        *validp = BUNDLE.num;
        return(prices);
    }
    return(0);
}

struct prices777 *prices777_makebasket(char *basketstr,cJSON *_basketjson,int32_t addbasket,char *typestr)
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
        printf(">>>>> (%s/%s).%s %llu %llu\n",refbase,refrel,typestr,(long long)refbaseid,(long long)refrelid);
        InstantDEX_name(key,&keysize,typestr,refname,refbase,&refbaseid,refrel,&refrelid);
        if ( addbasket != 0 )
        {
            prices777_addbundle(&valid,0,0,typestr,refbaseid,refrelid);
            printf("<<<<< valid.%d refname.(%s) (%s/%s).%s %llu %llu\n",valid,refname,refbase,refrel,typestr,(long long)refbaseid,(long long)refrelid);
        } else valid = 0;
        if ( valid >= 0 )
        {
            if ( (prices = prices777_createbasket(addbasket,refname,refbase,refrel,refbaseid,refrelid,basket,n,typestr)) != 0 )
            {
                if ( addbasket != 0 )
                    BUNDLE.ptrs[BUNDLE.num++] = prices;
                //prices->lastprice = prices777_basket(prices,MAX_DEPTH);
                printf("C total polling.%d added.(%s/%s).%s updating basket lastprice %f changed.%p %d groupsize.%d numgroups.%d %p\n",BUNDLE.num,prices->base,prices->rel,prices->exchange,prices->lastprice,&prices->changed,prices->changed,prices->basket[0].groupsize,prices->numgroups,&prices->basket[0].groupsize);
            }
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
    sprintf(numstr,"%llu",(long long)quoteid), cJSON_AddItemToArray(inner,cJSON_CreateString(numstr));
    cJSON_AddItemToArray(inner,cJSON_CreateNumber(timestamp));
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
                free_json(json);
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

int32_t match_unconfirmed(char *account,uint64_t quoteid,cJSON *txobj)
{
    return(-1);
}

double prices777_unconfNXT(struct prices777 *prices,int32_t maxdepth)
{
    char url[1024],account[1024],txidstr[1024],comment[1024],*str; uint32_t timestamp; int32_t type,i,subtype,n;
    cJSON *json,*bids,*asks,*commentobj,*array,*txobj,*attachment;
    double price,vol; uint64_t assetid,accountid,quoteid,baseamount,relamount,qty,priceNQT,amount;
    bids = cJSON_CreateArray(), asks = cJSON_CreateArray();
    prices->lastbid = prices->lastask = 0.;
    prices->O.numbids = prices->O.numasks = 0;
    sprintf(url,"requestType=getUnconfirmedTransactions");
    if ( (str= issue_NXTPOST(url)) != 0 )
    {
        //printf("{%s}\n",str);
        if ( (json= cJSON_Parse(str)) != 0 )
        {
            if ( (array= jarray(&n,json,"unconfirmedTransactions")) != 0 )
            {
                for (i=0; i<n; i++)
                {
      //{"senderPublicKey":"45c9266036e705a9559ccbd2b2c92b28ea6363d2723e8d42433b1dfaa421066c","signature":"9d6cefff4c67f8cf4e9487122e5e6b1b65725815127063df52e9061036e78c0b49ba38dbfc12f03c158697f0af5811ce9398702c4acb008323df37dc55c1b43d","feeNQT":"100000000","type":2,"fullHash":"6a2cd914b9d4a5d8ebfaecaba94ef4e7d2b681c236a4bee56023aafcecd9b704","version":1,"phased":false,"ecBlockId":"887016880740444200","signatureHash":"ba8eee4beba8edbb6973df4243a94813239bf57b91cac744cb8d6a5d032d5257","attachment":{"quantityQNT":"50","priceNQT":"18503000003","asset":"13634675574519917918","version.BidOrderPlacement":1},"senderRS":"NXT-FJQN-8QL2-BMY3-64VLK","subtype":3,"amountNQT":"0","sender":"5245394173527769812","ecBlockHeight":495983,"deadline":1440,"transaction":"15611117574733507690","timestamp":54136768,"height":2147483647},{"senderPublicKey":"c42956d0a9abc5a2e455e69c7e65ff9a53de2b697e913b25fcb06791f127af06","signature":"ca2c3f8e32d3aa003692fef423193053c751235a25eb5b67c21aefdeb7a41d0d37bc084bd2e33461606e25f09ced02d1e061420da7e688306e76de4d4cf90ae0","feeNQT":"100000000","type":2,"fullHash":"51c04de7106a5d5a2895db05305b53dd33fa8b9935d549f765aa829a23c68a6b","version":1,"phased":false,"ecBlockId":"887016880740444200","signatureHash":"d76fce4c081adc29f7e60eba2a930ab5050dd79b6a1355fae04863dddf63730c","attachment":{"version.AskOrderPlacement":1,"quantityQNT":"11570","priceNQT":"110399999","asset":"979292558519844732"},"senderRS":"NXT-ANWW-C5BZ-SGSB-8LGZY","subtype":2,"amountNQT":"0","sender":"8033808554894054300","ecBlockHeight":495983,"deadline":1440,"transaction":"6511477257080258641","timestamp":54136767,"height":2147483647}],"requestProcessingTime":0}
                    if ( (txobj= jitem(array,i)) == 0 )
                        continue;
                    copy_cJSON(txidstr,cJSON_GetObjectItem(txobj,"transaction"));
                    copy_cJSON(account,cJSON_GetObjectItem(txobj,"account"));
                    if ( account[0] == 0 )
                        copy_cJSON(account,cJSON_GetObjectItem(txobj,"sender"));
                    accountid = calc_nxt64bits(account);
                    type = (int32_t)get_API_int(cJSON_GetObjectItem(txobj,"type"),-1);
                    subtype = (int32_t)get_API_int(cJSON_GetObjectItem(txobj,"subtype"),-1);
                    qty = amount = assetid = 0;
                    if ( (attachment= cJSON_GetObjectItem(txobj,"attachment")) != 0 )
                    {
                        timestamp = get_blockutime(juint(attachment,"height"));
                        amount = get_API_nxt64bits(cJSON_GetObjectItem(attachment,"amountNQT"));
                         assetid = get_API_nxt64bits(cJSON_GetObjectItem(attachment,"asset"));
                        comment[0] = 0;
                        qty = get_API_nxt64bits(cJSON_GetObjectItem(attachment,"quantityQNT"));
                        priceNQT = get_API_nxt64bits(cJSON_GetObjectItem(attachment,"priceNQT"));
                        baseamount = (qty * prices->ap_mult), relamount = (qty * priceNQT);
                        copy_cJSON(comment,jobj(attachment,"message"));
                        if ( comment[0] != 0 )
                        {
                            unstringify(comment);
                            if ( (commentobj= cJSON_Parse(comment)) != 0 )
                            {
                                quoteid = get_API_nxt64bits(cJSON_GetObjectItem(commentobj,"quoteid"));
                                if ( strcmp(SUPERNET.NXTADDR,account) == 0 || Debuglevel > 2 )
                                    printf("acct.(%s) pending quoteid.%llu asset.%llu qty.%llu %.8f amount %.8f %d:%d tx.%s\n",account,(long long)quoteid,(long long)assetid,(long long)qty,dstr(priceNQT),dstr(amount),type,subtype,txidstr);
                                if ( quoteid != 0 )
                                    match_unconfirmed(account,quoteid,txobj);
                                free_json(commentobj);
                            }
                        }
                        quoteid = calc_nxt64bits(txidstr);
                        price = prices777_price_volume(&vol,baseamount,relamount);
                        if ( prices->baseid == assetid )
                        {
                            if ( Debuglevel > 2 )
                                printf("unconf.%d subtype.%d %s %llu (%llu %llu) %f %f mult.%llu qty.%llu pqt.%llu baseamount.%lld relamount.%lld\n",i,subtype,txidstr,(long long)prices->baseid,(long long)assetid,(long long)NXT_ASSETID,price,vol,(long long)prices->ap_mult,(long long)qty,(long long)priceNQT,(long long)baseamount,(long long)relamount);
                            if ( subtype == 2 )
                            {
                                array = bids;
                                prices->lastbid = price;
                            }
                            else if ( subtype == 3 )
                            {
                                array = asks;
                                prices->lastask = price;
                            }
                            cJSON_AddItemToArray(array,inner_json(price,vol,timestamp,quoteid,accountid,qty,priceNQT,baseamount,relamount));
                        }
                    }
                }
                free_json(json);
            }
            free(str);
        } else printf("cant get.(%s)\n",url);
    }
    json = cJSON_CreateObject();
    cJSON_AddItemToObject(json,"bids",bids);
    cJSON_AddItemToObject(json,"asks",asks);
    prices777_json_orderbook("unconf",prices,maxdepth,json,0,"bids","asks",0,0);
    if ( Debuglevel > 2 )//|| prices->O.numbids != 0 || prices->O.numasks != 0 )
        printf("%s %s/%s unconf.(%s) %f %f (%d %d)\n",prices->contract,prices->base,prices->rel,jprint(json,0),prices->lastbid,prices->lastask,prices->O.numbids,prices->O.numasks);
    free_json(json);
    return(_pairaved(prices->lastbid,prices->lastask));
}

double prices777_InstantDEX(struct prices777 *prices,int32_t maxdepth)
{
    cJSON *json; double hbla = 0.;
    if ( (json= InstantDEX_orderbook(prices)) != 0 )
    {
        if ( Debuglevel > 2 )
            printf("InstantDEX.(%s)\n",jprint(json,0));
        prices777_json_orderbook("InstantDEX",prices,maxdepth,json,0,"bids","asks",0,0);
        free_json(json);
    }
    return(hbla);
}

char *prices777_activebooks(char *name,char *_base,char *_rel,uint64_t baseid,uint64_t relid,int32_t maxdepth,int32_t allflag,int32_t tradeable)
{
    cJSON *basketjson,*array,*item,*item2; struct prices777 *active;
    uint64_t mgwbase,mgwrel; int32_t inverted,exchangeid; char numstr[64],base[64],rel[64],*retstr = 0;
    strcpy(base,_base), strcpy(rel,_rel);
    if ( (active= prices777_find(&inverted,baseid,relid,(tradeable != 0) ? "active" : "basket")) == 0 )
    {
        basketjson = cJSON_CreateObject(), array = cJSON_CreateArray();
        jaddstr(basketjson,"name",name), jaddstr(basketjson,"base",base), jaddstr(basketjson,"rel",rel);
        item = cJSON_CreateObject(), jaddstr(item,"exchange",INSTANTDEX_NAME), jaddi(array,item);
        mgwbase = is_MGWcoin(base), mgwrel = is_MGWcoin(rel);
        if ( (strcmp(base,"NXT") == 0 || strcmp(rel,"NXT") == 0) && (mgwbase != 0 || mgwrel != 0) )
        {
            item = cJSON_CreateObject(), jaddstr(item,"exchange",INSTANTDEX_NXTAENAME), jaddstr(item,"rel","NXT");
            item2 = cJSON_CreateObject(), jaddstr(item2,"exchange",INSTANTDEX_NXTAEUNCONF), jaddstr(item2,"rel","NXT");
            if ( strcmp(base,"NXT") == 0 )
            {
                jaddnum(item,"wt",-1), jaddnum(item2,"wt",-1);
                sprintf(numstr,"%llu",(long long)mgwrel), jaddstr(item,"baseid",numstr), jaddstr(item2,"baseid",numstr);
            }
            else
            {
                strcpy(base,_rel), jaddstr(item,"base",base), jaddstr(item2,"base",base);
                sprintf(numstr,"%llu",(long long)mgwbase), jaddstr(item,"baseid",numstr), jaddstr(item2,"baseid",numstr);
            }
            jaddi(array,item), jaddi(array,item2);
        }
        for (exchangeid=4; exchangeid<MAX_EXCHANGES; exchangeid++)
        {
            if ( Exchanges[exchangeid].name[0] == 0 )
                break;
            printf("exchangeid.%d ptr.%p (%s/%s)\n",exchangeid,Exchanges[exchangeid].supports,base,rel);
            if ( Exchanges[exchangeid].supports != 0 && (inverted= (*Exchanges[exchangeid].supports)(base,rel)) != 0 && (tradeable == 0 ||Exchanges[exchangeid].apikey[0] != 0) )
            {
                item = cJSON_CreateObject(), jaddstr(item,"exchange",Exchanges[exchangeid].name);
                if ( inverted < 0 )
                    jaddnum(item,"wt",-1);
                jaddi(array,item);
            }
        }
        jadd(basketjson,"basket",array);
        if ( (active= prices777_makebasket(0,basketjson,1,(tradeable != 0) ? "active" : "basket")) != 0 )
        {
            prices777_basket(active,maxdepth);
            retstr = prices777_orderbook_jsonstr(0,SUPERNET.my64bits,active,&active->O,maxdepth,allflag);
        } else retstr = clonestr("{\"error\":\"cant create active orderbook\"}");
        printf("BASKET.(%s)\n",jprint(basketjson,1));
        //free_json(basketjson);
    }
    else if ( (retstr= active->orderbook_jsonstrs[0][allflag]) == 0 )
        retstr = prices777_orderbook_jsonstr(0,SUPERNET.my64bits,active,&active->O,maxdepth,allflag);
    if ( retstr != 0 )
        retstr = clonestr(retstr);
    return(retstr);
}

#endif

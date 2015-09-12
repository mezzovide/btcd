/******************************************************************************
 * Copyright © 2014-2015 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * Nxt software, including this file, may be copied, modified, propagated,    *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#define BUNDLED
#define PLUGINSTR "shuffle"
#define PLUGNAME(NAME) shuffle ## NAME
#define STRUCTNAME struct PLUGNAME(_info) 
#define STRINGIFY(NAME) #NAME
#define PLUGIN_EXTRASIZE sizeof(STRUCTNAME)

#define DEFINES_ONLY
#include "../includes/portable777.h"
#include "../coins/coins777.c"
#include "../utils/ramcoder.c"
#include "plugin777.c"
#undef DEFINES_ONLY

STRUCTNAME
{
    uint32_t timestamp,numaddrs;
    uint64_t basebits,addrs[64],shuffleid,quoteid,fee;
    char base[16],destaddr[64],changeaddr[64],inputtxid[128],*vinstr,*voutstr,*changestr;
    int32_t vin,myind; uint64_t change,amount;
    struct cointx_info *T;
} *SHUFFLES[1000];

int32_t shuffle_idle(struct plugin_info *plugin) { return(0); }

int32_t shuffle_encrypt(uint64_t destbits,uint8_t *dest,uint8_t *src,int32_t len)
{
    uint8_t *cipher; bits256 destpubkey,onetime_pubkey,onetime_privkey,seed; HUFF H,*hp = &H; char destNXT[64]; int32_t haspubkey,cipherlen;
    expand_nxt64bits(destNXT,destbits);
    destpubkey = issue_getpubkey(&haspubkey,destNXT);
    if ( haspubkey == 0 )
    {
        printf("%s doesnt have pubkey, cant shuffle with him\n",destNXT);
        return(-1);
    }
    crypto_box_keypair(onetime_pubkey.bytes,onetime_privkey.bytes);
    if ( (cipher= encode_str(&cipherlen,src,len,destpubkey,onetime_privkey,onetime_pubkey)) != 0 )
    {
        memset(seed.bytes,0,sizeof(seed));
        seed.bytes[0] = 1;
        memset(dest,0,cipherlen*2);
        _init_HUFF(hp,cipherlen*2,dest);
        ramcoder_encoder(0,1,cipher,cipherlen,hp,&seed);
        cipherlen = hconv_bitlen(hp->bitoffset);
        free(cipher);
    } else cipherlen = 0;
    if ( 0 )
    {
        uint8_t check[8192]; int32_t newlen;
        //seed = curve25519(onetime_privkey,destpubkey);
        init_hexbytes_noT((void *)check,hp->buf,hconv_bitlen(hp->bitoffset));
        printf("cipherlen.%d -> numbits.%d bytes.%d [(%s)]\n",cipherlen,hp->bitoffset,hp->bitoffset/8,(void *)check);
        newlen = ramcoder_decoder(0,1,check,sizeof(check),hp,&seed);
        if ( newlen != cipherlen || memcmp(check,cipher,cipherlen) != 0 )
            printf("ramcoder error newlen.%d vs %d\n",newlen,cipherlen);
    }
    if ( 0 )
    {
        char hexstr[8192]; uint8_t buf[8192]; bits256 pubkey,privkey;
        crypto_box_keypair(pubkey.bytes,privkey.bytes);
        cipher = encode_str(&cipherlen,src,len,pubkey,onetime_privkey,onetime_pubkey);
        init_hexbytes_noT(hexstr,cipher,cipherlen);
        printf(" encrypted -> (%s).%d\n",hexstr,cipherlen);
        decode_cipher(buf,cipher,&cipherlen,privkey.bytes);
        init_hexbytes_noT(hexstr,buf,cipherlen);
        printf("decrypted.(%s)\n",hexstr);
    }
    return(cipherlen);
}

int32_t shuffle_decrypt(uint64_t nxt64bits,uint8_t *dest,uint8_t *src,int32_t len)
{
    bits256 seed; uint8_t buf[32768]; int32_t newlen = -1; HUFF H,*hp = &H;
    if ( nxt64bits == SUPERNET.my64bits )
    {
        memset(seed.bytes,0,sizeof(seed)), seed.bytes[0] = 1;
        _init_HUFF(hp,len*2,dest);
        newlen = ramcoder_decoder(0,1,buf,sizeof(buf),hp,&seed);
        if ( decode_cipher((void *)dest,buf,&newlen,SUPERNET.myprivkey) != 0 )
            printf("shuffle_decrypt Error: decode_cipher error len.%d -> newlen.%d\n",len,newlen);
    } else printf("cant decrypt another accounts packet\n");
    return(newlen);
}

uint64_t shuffle_txfee(struct coin777 *coin,int32_t numvins,int32_t numvouts)
{
    int32_t estimatedsize,incr;
    estimatedsize = (numvins * 130) + (numvouts * 50);
    incr = (estimatedsize / 200);
    return(coin->mgw.txfee * (incr+1));
}

char *shuffle_onetimeaddress(char *pubkey,struct coin777 *coin,char *account)
{
    char coinaddr[128],acctstr[128],*retstr; struct destbuf tmp;
    sprintf(acctstr,"\"%s\"",account);
    pubkey[0] = 0;
    if ( (retstr= bitcoind_passthru(coin->name,coin->serverport,coin->userpass,"getnewaddress",acctstr)) != 0 )
    {
        strcpy(coinaddr,retstr);
        if ( get_pubkey(&tmp,coin->name,coin->serverport,coin->userpass,coinaddr) > 0 )
            strcpy(pubkey,tmp.buf);
    }
    return(retstr);
}

int32_t shuffle_peel(char *peeled[],cJSON *strs,int32_t num)
{
    int32_t i,len,n; void *dest,*src; char *hexstr,*str;
    for (i=0; i<num; i++)
    {
        if ( (str= jstr(jitem(strs,i),0)) != 0 )
        {
            len = (int32_t)strlen(str) >> 1;
            src = calloc(1,len+16);
            dest = calloc(1,len+16);
            decode_hex(src,len,str);
            n = shuffle_decrypt(SUPERNET.my64bits,dest,src,len);
            hexstr = calloc(1,n*2+1);
            init_hexbytes_noT(hexstr,dest,n);
            free(src), free(dest);
            peeled[i] = hexstr;
        }
        else
        {
            printf("shuffle_peel: cant extract strs[%d]\n",i);
            return(-1);
        }
    }
    return(i);
}

char *shuffle_layer(char *str,uint64_t *addrs,int32_t num)
{
    int32_t i,n,len; uint8_t data[8192],dest[8192];
    len = (int32_t)strlen(str) >> 1;
    if ( len > sizeof(dest)/2 )
    {
        printf("shuffle_layer str.(%s) is too big\n",str);
        return(0);
    }
    decode_hex(data,len,str);
    printf("layer.(%s) len.%d num.%d %llx\n",str,len,num,*(long long *)data);
    if ( num > 0 )
    {
        for (i=num-1; i>=0; i--)
        {
            n = shuffle_encrypt(addrs[i],dest,data,len);
            memcpy(data,dest,n);
            len = n;
        }
    }
    init_hexbytes_noT((char *)dest,data,len);
    printf("(%s) newlen.%d\n",(char *)dest,len);
    return(clonestr((char *)dest));
}

char *shuffle_onetime(char *pubkey,struct coin777 *coin,char *type,uint64_t *addrs,int32_t num)
{
    char *newaddress,*retstr = 0;
    if ( (newaddress= shuffle_onetimeaddress(pubkey,coin,type)) != 0 )
    {
        strcpy(pubkey,newaddress);
        retstr = shuffle_layer(pubkey,addrs,num);
        free(newaddress);
    }
    return(retstr);
}

char *shuffle_vin(uint64_t *changep,char *txid,int32_t *vinp,struct coin777 *coin,uint64_t amount,uint64_t *addrs,int32_t num)
{
    uint64_t total; int32_t n; struct rawvin vin; struct subatomic_unspent_tx *utx,*up; char buf[512],*retstr = 0;
    memset(&vin,0,sizeof(vin));
    *changep = 0;
    if ( (utx= gather_unspents(&total,&n,coin,0)) != 0  )
    {
        if ( (up= subatomic_bestfit(coin,utx,n,amount,1)) != 0 )
        {
            *changep = (up->amount - amount);
            *vinp = up->vout;
            strcpy(txid,up->txid.buf);
            sprintf(buf,"%02x%s",up->vout,up->txid.buf);
            printf("shuffle_vin.(%s)\n",buf);
            retstr = shuffle_layer(buf,addrs,num);
        } else printf("no bestfits: %p\n",up);
        free(utx);
    }
    return(retstr);
}

char *shuffle_vout(char *destaddr,struct coin777 *coin,char *type,uint64_t amount,uint64_t *addrs,int32_t num)
{
    char buf[512],pubkey[128],hexaddress[64],*destaddress,*retstr = 0; uint8_t rmd160[21];
    if ( (destaddress= shuffle_onetimeaddress(pubkey,coin,type)) != 0 )
    {
        btc_convaddr(hexaddress,destaddress);
        decode_hex(rmd160,21,hexaddress);
        //btc_convrmd160(testaddr,rmd160[0],rmd160+1);
        sprintf(buf,"%016llx%s",(long long)amount,hexaddress);
        retstr = shuffle_layer(buf,addrs,num);
        strcpy(destaddr,destaddress);
        free(destaddress);
    }
    return(retstr);
}

char *shuffle_cointx(struct coin777 *coin,char *vins[],int32_t numvins,char *vouts[],int32_t numvouts)
{
    struct cointx_info T; int32_t i; cJSON *item; char *txid,*coinaddr,txbytes[65536]; uint64_t totaloutputs,totalinputs,value,fee,sharedfee;
    memset(&T,0,sizeof(T));
    T.version = 1;
    T.timestamp = (uint32_t)time(NULL);
    totaloutputs = totalinputs = 0;
    for (i=0; i<numvins; i++)
    {
        if ( (item= cJSON_Parse(vins[i])) != 0 )
        {
            if ( is_cJSON_Array(item) != 0 && cJSON_GetArraySize(item) == 2 )
            {
                if ( (txid= jstr(jitem(item,0),0)) != 0 )
                {
                    safecopy(T.inputs[T.numinputs].tx.txidstr,txid,sizeof(T.inputs[i].tx.txidstr));
                    T.inputs[T.numinputs].tx.vout = juint(jitem(item,1),0);
                    T.inputs[T.numinputs].sequence = 0xffffffff;
                    if ( (value= ram_verify_txstillthere(coin->name,coin->serverport,coin->userpass,txid,T.inputs[T.numinputs].tx.vout)) > 0 )
                        totalinputs += value;
                    else
                    {
                        printf("error getting unspent.(%s v%d)\n",txid,T.inputs[T.numinputs].tx.vout);
                        free_json(item);
                        break;
                    }
                    T.numinputs++;
                }
            }
            free_json(item);
        }
    }
    if ( T.numinputs == numvins )
    {
        for (i=0; i<numvouts; i++)
        {
            if ( (item= cJSON_Parse(vouts[i])) != 0 )
            {
                if ( is_cJSON_Array(item) != 0 && cJSON_GetArraySize(item) == 2 )
                {
                    if ( (coinaddr= jstr(jitem(item,0),0)) != 0 )
                    {
                        safecopy(T.outputs[T.numoutputs].coinaddr,coinaddr,sizeof(T.outputs[T.numoutputs].coinaddr));
                        T.outputs[T.numoutputs].value = SATOSHIDEN * jdouble(jitem(item,1),0);
                        totaloutputs += T.outputs[T.numoutputs].value;
                        T.numoutputs++;
                    }
                }
                free_json(item);
            }
        }
        if ( T.numinputs == numvins )
        {
            fee = ((numvins >> 1) * coin->mgw.txfee) + (totalinputs >> 10);
            if ( totalinputs < totaloutputs+fee )
            {
                printf("not enough inputs %.8f for outputs %.8f + fee %.8f\n",dstr(totalinputs),dstr(totaloutputs),dstr(fee));
                return(0);
            }
            fee = shuffle_txfee(coin,numvins,numvouts);
            if ( totalinputs < totaloutputs+fee )
            {
                printf("not enough inputs %.8f for outputs %.8f + fee %.8f\n",dstr(totalinputs),dstr(totaloutputs),dstr(fee));
                return(0);
            }
            if ( (sharedfee= (totalinputs - totaloutputs) - fee) > numvouts )
            {
                if ( coin->donationaddress[0] != 0 )
                {
                    T.outputs[T.numoutputs].value = sharedfee;
                    strcpy(T.outputs[T.numoutputs].coinaddr,coin->donationaddress);
                    T.numoutputs++;
                }
                else
                {
                    printf("share excess fee %.8f among %d outputs\n",dstr(sharedfee),numvouts);
                    sharedfee /= numvouts;
                    for (i=0; i<numvouts; i++)
                        T.outputs[i].value += sharedfee;
                }
            }
            _emit_cointx(txbytes,sizeof(txbytes),&T,coin->mgw.oldtx_format);
            return(clonestr(txbytes));
        }
    }
    return(0);
}

int32_t shuffle_strs(char *ptrs[],uint8_t num)
{
    int32_t i; uint8_t r; char *tmp;
    for (i=0; i<num; i++)
    {
        randombytes(&r,sizeof(r));
        r %= num;
        tmp = ptrs[i];
        ptrs[i] = ptrs[r];
        ptrs[r] = tmp;
    }
    return(i);
}

cJSON *shuffle_strarray(char *ptrs[],int32_t num)
{
    cJSON *array = cJSON_CreateArray(); int32_t i;
    for (i=0; i<num; i++)
        jaddistr(array,ptrs[i]);
    return(array);
}

char *shuffle_validate(struct coin777 *coin,char *rawtx,struct shuffle_info *sp)
{
    struct cointx_info *cointx; uint32_t nonce; int32_t i,vin=-1,vout=-1,changeout=-1; char buf[8192],sigstr[4096],*str;
    if ( sp == 0 )
    {
        printf("cant find shuffleid.%llu\n",(long long)sp->shuffleid);
        return(clonestr("{\"error\":\"cant find shuffleid\"}"));
    }
    if ( (cointx= _decode_rawtransaction(rawtx,coin->mgw.oldtx_format)) != 0 )
    {
        for (i=0; i<cointx->numoutputs; i++)
        {
            if ( vout < 0 && strcmp(cointx->outputs[i].coinaddr,sp->destaddr) == 0 )
            {
                if ( cointx->outputs[i].value == sp->amount )
                    vout = i;
                else
                {
                    printf("amount mismatch %.8f vs %.8f\n",dstr(cointx->outputs[i].value),dstr(sp->amount));
                    break;
                }
            }
            if ( sp->change != 0 && changeout < 0 && strcmp(cointx->outputs[i].coinaddr,sp->changeaddr) == 0 )
            {
                if ( cointx->outputs[i].value == sp->change )
                    changeout = i;
                else
                {
                    printf("change mismatch %.8f vs %.8f\n",dstr(cointx->outputs[i].value),dstr(sp->change));
                    break;
                }
            }
            if ( (sp->change == 0 || changeout >= 0) && vout >= 0 )
                break;
        }
        if ( (sp->change == 0 || changeout >= 0) && vout >= 0 )
        {
            for (i=0; i<cointx->numinputs; i++)
            {
                if ( vin < 0 && strcmp(cointx->inputs[i].tx.txidstr,sp->inputtxid) == 0 )
                {
                    if ( cointx->inputs[i].tx.vout == sp->vin )
                    {
                        vin = i;
                        break;
                    }
                    else
                    {
                        printf("vout mismatch %d vs %d\n",cointx->inputs[i].tx.vout,sp->vin);
                        break;
                    }
                }
            }
            if ( vin >= 0 )
            {
                if ( shuffle_signvin(sigstr,coin,cointx,vin) != 0 )
                {
                    sprintf(buf,"{\"shuffleid\":\"%llu\",\"timestamp\":\"%u\",\"plugin\":\"relay\",\"destplugin\":\"shuffle\",\"method\":\"busdata\",\"submethod\":\"signed\",\"sig\":\"%s\",\"vin\":%d}",(long long)sp->shuffleid,sp->timestamp,sigstr,vin);
                    if ( (str= busdata_sync(&nonce,buf,"allnodes",0)) != 0 )
                        free(str);
                    return(clonestr(buf));
                }
            }
        }
        sp->T = cointx;
    }
    return(clonestr("{\"error\":\"shuffle tx invalid\"}"));
}

struct shuffle_info *shuffle_create(int32_t *createdflagp,char *base,uint32_t timestamp,uint64_t *addrs,int32_t numaddrs)
{
    struct shuffle_info *sp = 0; bits256 hash; int32_t i,firstslot = -1;
    if ( createdflagp != 0 )
        *createdflagp = 0;
    for (i=0; i<numaddrs; i++)
        if ( addrs[i] == SUPERNET.my64bits )
            break;
    if ( i == numaddrs )
    {
        printf("this node not in addrs\n");
        return(0);
    }
    if ( numaddrs > 0 && (sp= calloc(1,sizeof(*sp))) != 0 )
    {
        sp->myind = i;
        strcpy(sp->base,base);
        if ( (sp->timestamp= timestamp) == 0 )
            sp->timestamp = (uint32_t)time(NULL);
        sp->numaddrs = numaddrs;
        sp->basebits = stringbits(base);
        memcpy(sp->addrs,addrs,numaddrs * sizeof(sp->addrs[0]));
        calc_sha256(0,hash.bytes,(uint8_t *)sp,numaddrs * sizeof(sp->addrs[0]) + 2*sizeof(uint64_t));
        sp->shuffleid = hash.txid;
        for (i=0; i<sizeof(SHUFFLES)/sizeof(*SHUFFLES); i++)
        {
            if ( SHUFFLES[i] != 0 )
            {
                if ( sp->shuffleid == SHUFFLES[i]->shuffleid )
                {
                    printf("shuffleid %llu already exists!\n",(long long)sp->shuffleid);
                    free(sp);
                    return(SHUFFLES[i]);
                }
            }
            else if ( firstslot < 0 )
                firstslot = i;
        }
        SHUFFLES[firstslot] = sp;
        if ( createdflagp != 0 )
            *createdflagp = 1;
    }
    return(sp);
}

struct shuffle_info *shuffle_find(uint64_t shuffleid)
{
    int32_t i;
    for (i=0; i<sizeof(SHUFFLES)/sizeof(*SHUFFLES); i++)
        if ( SHUFFLES[i] != 0 && shuffleid == SHUFFLES[i]->shuffleid )
            return(SHUFFLES[i]);
    return(0);
}

char *shuffle_start(char *base,uint32_t timestamp,uint64_t *addrs,int32_t num)
{
    cJSON *array; struct InstantDEX_quote *iQ = 0; char destNXT[64],buf[2048],*addrstr;
    int32_t createdflag,i,j,n,r,haspubkey,myind = -1; uint32_t now;
    uint64_t _addrs[64],quoteid = 0; struct shuffle_info *sp; struct coin777 *coin;
    if ( base == 0 || base[0] == 0 )
        return(clonestr("{\"error\":\"no base defined\"}"));
    coin = coin777_find(base,1);
    now = (uint32_t)time(NULL);
    if ( timestamp != 0 && now > timestamp+777 )
        return(clonestr("{\"error\":\"shuffle expired\"}"));
    if ( addrs == 0 )
    {
        addrs = _addrs, num = 0;
        if ( (array= InstantDEX_shuffleorders(&quoteid,SUPERNET.my64bits,base)) != 0 )
        {
            if ( (n= cJSON_GetArraySize(array)) > 0 )
            {
                r = (rand() % n);
                for (j=0; j<n; j++)
                {
                    i = (j + r) % n;
                    if ( (addrs[num]= j64bits(jitem(array,i),0)) != 0 )
                    {
                        if ( addrs[num] == SUPERNET.my64bits )
                            myind = num;
                        expand_nxt64bits(destNXT,addrs[num]);
                        issue_getpubkey(&haspubkey,destNXT);
                        num += haspubkey;
                        if ( num == sizeof(_addrs)/sizeof(*_addrs) )
                            break;
                    }
                    printf("i.%d j.%d num.%d %llu\n",i,j,num,(long long)addrs[num-1]);
                }
            }
            free_json(array);
        }
    }
printf("shuffle_start(%s) addrs.%p num.%d\n",base,addrs,num);
    if ( (i= myind) > 0 )
    {
        addrs[i] = addrs[0];
        addrs[0] = SUPERNET.my64bits;
        i = 0;
    }
    if ( (sp= shuffle_create(&createdflag,base,timestamp,addrs,num)) == 0 )
    {
        printf("cant create shuffle.(%s) numaddrs.%d\n",base,num);
        return(clonestr("{\"error\":\"cant create shuffleid\"}"));
    }
    if ( createdflag != 0 && sp->myind == 0 && addrs[sp->myind] == SUPERNET.my64bits )
    {
        if ( quoteid == 0 )
        {
            if ( (array= InstantDEX_shuffleorders(&quoteid,SUPERNET.my64bits,base)) != 0 )
                free_json(array);
        }
        if ( (iQ= find_iQ(quoteid)) != 0 )
        {
            sp->amount = iQ->s.baseamount;
            iQ->s.pending = 1;
            sp->fee = ((sp->amount>>10) < coin->mgw.txfee) ? coin->mgw.txfee : (sp->amount>>10);
            sp->vinstr = shuffle_vin(&sp->change,sp->inputtxid,&sp->vin,coin,sp->amount + sp->fee + coin->mgw.txfee,&addrs[i+1],num-i-1);
            if ( sp->change != 0 )
                sp->changestr = shuffle_vout(sp->changeaddr,coin,"change",sp->change,&addrs[i+1],num-i-1);
            sp->voutstr = shuffle_vout(sp->destaddr,coin,"shuffled",sp->amount,&addrs[i+1],num-i-1);
            if ( sp->amount == 0 || coin == 0 || sp->vinstr == 0 || sp->voutstr == 0 )
            {
                printf("num.%d amount %.8f (%s) vinstr.(%s) voutstr.(%s)\n",num,dstr(sp->amount),coin->name,sp->vinstr,sp->voutstr);
                return(clonestr("{\"error\":\"this node not shuffling due to calc error\"}"));
            }
            else
            {
                array = cJSON_CreateArray();
                for (j=0; j<num; j++)
                    jaddi64bits(array,addrs[j]);
                addrstr = jprint(array,1);
                sprintf(buf,"{\"shuffleid\":\"%llu\",\"timestamp\":%u,\"base\":\"%s\",\"vins\":[\"%s\"],\"vouts\":[\"%s\"],\"addrs\":%s}",(long long)sp->shuffleid,sp->timestamp,sp->base,sp->vinstr,sp->voutstr,addrstr);
                free(addrstr);
                expand_nxt64bits(destNXT,addrs[i + 1]);
                printf("destNXT.(%s) addrs[%d] %llu\n",destNXT,i+1,(long long)addrs[i+1]);
                //if ( strlen(buf) >= sizeof(hexstr)>>1 )
                //    return(clonestr("{\"error\":\"shuffle buffer overflow\"}"));
                //init_hexbytes_noT(hexstr,(void *)buf,strlen(buf)>>1);
                telepathic_PM(destNXT,buf);
                return(clonestr("{\"success\":\"shuffle created\"}"));
            }
        }
    }
    return(clonestr("{\"success\":\"shuffle already there\"}"));
}

int32_t shuffle_incoming(char *jsonstr)
{
    struct coin777 *coin = 0; cJSON *newjson,*json,*vins,*vouts,*array; int32_t i,j,num,createdflag,numvins,numvouts; struct shuffle_info *sp;
    char *newvins[1024],*newvouts[1024],destNXT[64],buf[8192],*base,*str,*txbytes,*msg; uint64_t shuffleid; uint32_t nonce;
    uint64_t addrs[64];
    if ( (json= cJSON_Parse(jsonstr)) != 0 && (base= jstr(json,"base")) != 0 && (shuffleid= j64bits(json,"shuffleid")) != 0 )
    {
        coin = coin777_find(base,0);
        if ( (sp= shuffle_find(shuffleid)) == 0 )
        {
            if ( (array= jarray(&num,json,"addrs")) != 0 )
                for (i=0; i<num; i++)
                    addrs[i] = j64bits(jitem(array,i),0);
            sp = shuffle_create(&createdflag,base,juint(json,"timestamp"),addrs,num);
            if ( sp != 0 && sp->shuffleid != shuffleid )
            {
                printf("shuffleid mismatch %llu vs %llu\n",(long long)sp->shuffleid,(long long)shuffleid);
                free_json(json);
                return(-1);
            }
        }
        if ( sp != 0 && strcmp(sp->base,base) == 0 && coin != 0 && (vins= jarray(&numvins,json,"vins")) != 0 && (vouts= jarray(&numvouts,json,"vouts")) != 0 )
        {
            if ( numvins < sizeof(newvins)/sizeof(*newvins)-2 && numvouts < sizeof(newvouts)/sizeof(*newvouts)-2 )
            {
                shuffle_peel(newvins,vins,numvins), newvins[numvins++] = clonestr(sp->vinstr);
                shuffle_peel(newvouts,vouts,numvouts), newvouts[numvouts++] = clonestr(sp->voutstr);
                if ( sp->change != 0 )
                    newvouts[numvouts++] = clonestr(sp->changestr);
                for (j=0; j<13; j++)
                    shuffle_strs(newvins,numvins);
                for (j=0; j<13; j++)
                    shuffle_strs(newvouts,numvouts);
                if ( sp->myind == sp->numaddrs-1 )
                {
                    if ( (txbytes= shuffle_cointx(coin,newvins,numvins,newvouts,numvouts)) != 0 )
                    {
                        sprintf(buf,"{\"shuffleid\":\"%llu\",\"timestamp\":\"%u\",\"plugin\":\"relay\",\"destplugin\":\"shuffle\",\"method\":\"busdata\",\"submethod\":\"validate\",\"rawtx\":\"%s\"}",(long long)sp->shuffleid,sp->timestamp,txbytes);
                        if ( (str= busdata_sync(&nonce,buf,"allnodes",0)) != 0 )
                            free(str);
                        free(txbytes);
                    }
                }
                else
                {
                    newjson = cJSON_CreateObject();
                    vins = shuffle_strarray(newvins,numvins), vouts = shuffle_strarray(newvouts,numvouts);
                    jadd(newjson,"vins",vins), jadd(newjson,"vouts",vouts);
                    msg = jprint(newjson,1);
                    expand_nxt64bits(destNXT,sp->addrs[sp->myind+1]);
                    telepathic_PM(destNXT,msg);
                    free(msg);
                    free_json(newjson);
                }
                for (i=0; i<numvins; i++)
                    free(newvins[i]);
                for (i=0; i<numvouts; i++)
                    free(newvouts[i]);
            }
        } else printf("shuffle_incoming: missing sp.%p or coin.%p\n",sp,coin);
        free_json(json);
    }
    return(0);
}

#define SHUFFLE_METHODS "validate", "signed", "start"
char *PLUGNAME(_methods)[] = { SHUFFLE_METHODS };
char *PLUGNAME(_pubmethods)[] = { SHUFFLE_METHODS };
char *PLUGNAME(_authmethods)[] = { SHUFFLE_METHODS };

uint64_t PLUGNAME(_register)(struct plugin_info *plugin,STRUCTNAME *data,cJSON *argjson)
{
    uint64_t disableflags = 0;
    printf("init %s size.%ld\n",plugin->name,sizeof(struct shuffle_info));
    plugin->allowremote = 1;
    return(disableflags); // set bits corresponding to array position in _methods[]
}

int32_t PLUGNAME(_process_json)(char *forwarder,char *sender,int32_t valid,struct plugin_info *plugin,uint64_t tag,char *retbuf,int32_t maxlen,char *jsonstr,cJSON *json,int32_t initflag,char *tokenstr)
{
    char *resultstr,tx[8192],*methodstr,*rawtx,*sig,*cointxid,*retstr = 0; int32_t i,vin; uint64_t shuffleid; struct coin777 *coin; struct shuffle_info *sp;
    retbuf[0] = 0;
    plugin->allowremote = 1;
    if ( initflag > 0 )
    {
        /*if ( 0 && (jsonstr= loadfile(&allocsize,"SuperNET.conf")) != 0 )
        {
            if ( (json= cJSON_Parse(jsonstr)) != 0 )
                SuperNET_initconf(json), free_json(json);
            free(jsonstr);
        }*/
        strcpy(retbuf,"{\"result\":\"shuffle init\"}");
    }
    else
    {
        fprintf(stderr,"<<<<<<<<<<<< INSIDE PLUGIN! process %s (%s)\n",plugin->name,jsonstr);
        resultstr = cJSON_str(cJSON_GetObjectItem(json,"result"));
        methodstr = cJSON_str(cJSON_GetObjectItem(json,"method"));
        retbuf[0] = 0;
        if ( resultstr != 0 && strcmp(resultstr,"registered") == 0 )
        {
            plugin->registered = 1;
            strcpy(retbuf,"{\"result\":\"activated\"}");
            return((int32_t)strlen(retbuf));
        }
        if ( plugin_result(retbuf,json,tag) > 0 )
            return((int32_t)strlen(retbuf));
        if ( methodstr == 0 || methodstr[0] == 0 )
        {
            printf("(%s) has not method\n",jsonstr);
            return(0);
        }
        else if ( strcmp(methodstr,"start") == 0 )
        {
            retstr = shuffle_start(jstr(json,"base"),0,0,0);
        }
        else if ( strcmp(methodstr,"validate") == 0 )
        {
            if ( (rawtx= jstr(json,"rawtx")) != 0 && (shuffleid= j64bits(json,"shuffleid")) != 0 && (sp= shuffle_find(shuffleid)) != 0 )
            {
                if ( (coin= coin777_find(sp->base,0)) != 0 )
                    retstr = shuffle_validate(coin,rawtx,sp);
            }
            if ( retstr == 0 )
                retstr = clonestr("{\"error\":\"shuffle validate invalid args\"}");
        }
        else if ( strcmp(methodstr,"signed") == 0 )
        {
            if ( (sig= jstr(json,"sig")) != 0 && (shuffleid= j64bits(json,"shuffleid")) != 0 && (sp= shuffle_find(shuffleid)) != 0 )
            {
                if ( (coin= coin777_find(sp->base,0)) != 0 && (vin= juint(json,"vin")) >= 0 && vin < sp->T->numinputs && strlen(sig) < sizeof(sp->T->inputs[0].sigs) )
                {
                    strcpy(sp->T->inputs[vin].sigs,sig);
                    if ( 0 )//shuffle_verify(coin,sp->T,vin) < 0 )
                        retstr = clonestr("{\"error\":\"shuffle verification failed\"}");
                    else
                    {
                        for (i=0; i<sp->T->numinputs; i++)
                            if ( sp->T->inputs[vin].sigs[0] == 0 )
                                break;
                        if ( i == sp->T->numinputs )
                        {
                            _emit_cointx(tx+2,sizeof(tx)-2,sp->T,coin->mgw.oldtx_format);
                            strcat(tx,"\"]");
                            if ( (cointxid= bitcoind_passthru(coin->name,coin->serverport,coin->userpass,"sendrawtransaction",tx)) != 0 )
                            {
                                printf(">>>>>>>>>>>>> %s BROADCAST.(%s) (%s)\n",coin->name,tx,cointxid);
                                free(cointxid);
                            } else printf("error sending transaction.(%s)\n",tx);
                            delete_iQ(sp->quoteid);
                        } else retstr = clonestr("{\"success\":\"shuffle accepted sig\"}");
                    }
                }
            }
            if ( retstr == 0 )
                retstr = clonestr("{\"error\":\"shuffle signed invalid args\"}");
        }
    }
    return(plugin_copyretstr(retbuf,maxlen,retstr));
}

int32_t PLUGNAME(_shutdown)(struct plugin_info *plugin,int32_t retcode)
{
    if ( retcode == 0 )  // this means parent process died, otherwise _process_json returned negative value
    {
    }
    return(retcode);
}
#include "plugin777.c"

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
#define PLUGINSTR "dcnet"
#define PLUGNAME(NAME) dcnet ## NAME
#define STRUCTNAME struct PLUGNAME(_info)
#define STRINGIFY(NAME) #NAME
#define PLUGIN_EXTRASIZE sizeof(STRUCTNAME)

#define DEFINES_ONLY
#include "../../utils/huffstream.c"
#include "../plugin777.c"
#undef DEFINES_ONLY

#define DCNET_API "join", "round"
char *PLUGNAME(_methods)[] = { DCNET_API };
char *PLUGNAME(_pubmethods)[] = { DCNET_API };
char *PLUGNAME(_authmethods)[] = { "" };

union _bits256 { uint8_t bytes[32]; uint16_t ushorts[16]; uint32_t uints[8]; uint64_t ulongs[4]; uint64_t txid; };
typedef union _bits256 bits256;
union _bits320 { uint8_t bytes[40]; uint16_t ushorts[20]; uint32_t uints[10]; uint64_t ulongs[5]; uint64_t txid; };
typedef union _bits320 bits320;

#define MAXNODES 64
#define MAXGROUPS 64
struct dcnode { bits320 pubexp,pubexp2; uint64_t id; uint32_t lastcontact; };
struct dcgroup { bits256 Ois[MAXNODES],commits[MAXNODES]; struct dcnode *nodes[MAXNODES]; bits320 prodOi,prodcommit; uint64_t id; uint32_t n,created,nonz,myind; };

STRUCTNAME
{
    uint64_t myid;
    char bind[128],connect[128];
    bits256 privkey,privkey2;
    bits320 pubexp,pubexp2;
    int32_t bus,mode,num,numgroups;
    struct dcgroup groups[MAXGROUPS];
    struct dcnode nodes[MAXNODES];
} DCNET;

uint64_t PLUGNAME(_register)(struct plugin_info *plugin,STRUCTNAME *data,cJSON *argjson)
{
    uint64_t disableflags = 0;
    plugin->allowremote = 1;
    return(disableflags); // set bits corresponding to array position in _methods[]
}
int32_t init_hexbytes_noT(char *hexbytes,uint8_t *message,long len);
int32_t safecopy(char *dest,char *src,long len);
char *clonestr(char *str);
int32_t decode_hex(unsigned char *bytes,int32_t n,char *hex);

#include "../../utils/curve25519.h"
bits320 Unit;

bits256 rand256(int32_t privkeyflag)
{
    bits256 randval;
    randombytes(randval.bytes,sizeof(randval));
    if ( privkeyflag != 0 )
        randval.bytes[0] &= 0xf8, randval.bytes[31] &= 0x7f, randval.bytes[31] |= 0x40;
    return(randval);
}

int32_t huff256(HUFF *hp,bits256 *data)
{
    // 0, 1, 2, ... 254, 255
    _init_HUFF(hp,sizeof(*data)-1,data->bytes); // 6 bits available in last byte used for desti, ie nodes position in group
    return(hseek(hp,3,SEEK_SET));
}

bits256 curve25519_jl777(bits256 secret,const bits256 basepoint)
{
    bits256 curve25519(bits256 secret,const bits256 basepoint);
    bits320 bp,x,z; bits256 check,pubkey; int32_t i;
    secret.bytes[0] &= 0xf8, secret.bytes[31] &= 0x7f, secret.bytes[31] |= 64;
    bp = fexpand(basepoint);
    cmult(&x,&z,secret,bp);
    z = fmul(x,crecip(z)); // x/z -> pubkey, secret * 9 -> (x,z)
    check = curve25519(secret,basepoint);
    pubkey = fcontract(z);
    if ( memcmp(check.bytes,pubkey.bytes,sizeof(check)) != 0 )
    {
        for (i=0; i<4; i++)
            printf("%016llx ",(long long)check.ulongs[i]);
        printf("check\n");
        for (i=0; i<4; i++)
            printf("%016llx ",(long long)pubkey.ulongs[i] ^ check.ulongs[i]);
        printf("pubkey\n");
        printf("compare error on pubkey %llx vs %llx\n",(long long)check.txid,(long long)pubkey.txid);
    }
    return(pubkey);
}

bits256 keypair(bits256 *pubkeyp)
{
    bits256 basepoint,privkey;
    memset(&basepoint,0,sizeof(basepoint));
    basepoint.bytes[0] = 9;
    privkey = rand256(1);
    *pubkeyp = curve25519_jl777(privkey,basepoint);
    return(privkey);
}

void dcinit()
{
    bits256 pubkey,pubkey2;
    DCNET.privkey = keypair(&pubkey), DCNET.privkey2 = keypair(&pubkey2);
    DCNET.myid = pubkey.txid;
    DCNET.pubexp = fexpand(pubkey), DCNET.pubexp2 = fexpand(pubkey2);
}

void set_dcnode(struct dcnode *node,bits320 pubexp,bits320 pubexp2)
{
    node->pubexp = pubexp, node->pubexp2 = pubexp2, node->lastcontact = (uint32_t)time(NULL);
    node->id = fcontract(pubexp).txid;
}

struct dcnode *find_dcnode(uint64_t txid)
{
    int32_t i;
    for (i=0; i<DCNET.num; i++)
    {
        if ( DCNET.nodes[i].id == txid )
            return(&DCNET.nodes[i]);
    }
    return(0);
}

struct dcgroup *find_dcgroup(uint64_t groupid)
{
    int32_t i;
    for (i=0; i<DCNET.numgroups; i++)
    {
        if ( DCNET.groups[i].id == groupid )
            return(&DCNET.groups[i]);
    }
    return(0);
}

bits320 dcround(bits320 *commitp,int32_t i,struct dcgroup *group,bits256 G,bits256 H,bits320 msgelement)
{
    int32_t j; bits320 prod,commit,c,x,z,x2,z2,shared,shared2;
    prod = msgelement;
    commit = Unit;
    for (j=0; j<group->n; j++)
    {
        memset(c.bytes,0,sizeof(c));
        if ( i != j )
        {
            cmult(&x,&z,DCNET.privkey,group->nodes[j]->pubexp);
            cmult(&x2,&z2,DCNET.privkey2,group->nodes[j]->pubexp2);
            if ( i < j )
                shared = fmul(x,crecip(z)), shared2 = fmul(x2,crecip(z2));
            else shared = fmul(z,crecip(x)), shared2 = fmul(z2,crecip(x2));
            cmult(&x,&z,G,shared);
            cmult(&x2,&z2,H,shared2);
            if ( i < j )
                c = fmul(fmul(x,crecip(z)),fmul(x2,crecip(z2)));
            else c = fmul(fmul(z,crecip(x)),fmul(z2,crecip(x2)));
            commit = fmul(commit,c);
            prod = fmul(shared,prod);
        }
        if ( i == 0 && j == 0 )
            printf("%016llx ",prod.txid);
        else printf("%016llx ",c.txid);
    }
    *commitp = commit;
    return(prod);
}

int32_t serialize_data(uint8_t *databuf,int32_t datalen,uint64_t val,int32_t size)
{
    int32_t i;
    if ( databuf != 0 )
        for (i=0; i<size; i++,val>>=8)
            databuf[datalen++] = (val & 0xff);
    return(datalen);
}

int32_t deserialize_data(uint8_t *databuf,int32_t datalen,uint64_t *valp,int32_t size)
{
    int32_t i; uint64_t val = 0;
    if ( databuf != 0 )
        for (i=0; i<size; i++)
            val |= ((uint64_t)databuf[datalen++] << ((size-i-1)<<3));
    *valp = val;
    return(datalen);
}

int32_t serialize_data256(uint8_t *databuf,int32_t datalen,bits256 data)
{
    int32_t i;
    if ( databuf != 0 )
        for (i=0; i<4; i++)
            datalen = serialize_data(databuf,datalen,data.ulongs[i],sizeof(data.ulongs[i]));
    return(datalen);
}

int32_t deserialize_data256(uint8_t *databuf,int32_t datalen,bits256 *data)
{
    int32_t i;
    if ( databuf != 0 )
        for (i=0; i<4; i++)
            datalen = deserialize_data(databuf,datalen,&data->ulongs[i],sizeof(data->ulongs[i]));
    return(datalen);
}

bits320 dcnet_message(struct dcgroup *group,int32_t groupsize)
{
    int32_t desti,i; char msgstr[32]; bits256 msg; HUFF H; bits320 msgelement = Unit;
    if ( (rand() % (groupsize*2)) == 0 )
    {
        memset(msgstr,0,sizeof(msgstr));
        strcpy(msgstr,"hello world");
        memset(&msg,0,sizeof(msg));
        huff256(&H,&msg);
        while ( (desti= (rand() % groupsize)) == group->myind )
            ;
        for (i=0; i<30&&msgstr[i]!=0; i++)
            hwrite(msgstr[i],8,&H);
        msg.bytes[31] |= (desti & 0x3f);
        printf("%llu node.%d sending msg to %d\n",(long long)DCNET.myid,group->myind,desti);
        msgelement = fexpand(msg);
    }
    return(msgelement);
}

void dcround_update(struct dcgroup *group,uint64_t sender,bits256 Oi,bits256 commit,void *packet,int32_t len)
{
    int32_t i,numbits,desti = -1; struct dcnode *node; char msgstr[33]; bits256 msg; HUFF H;
    printf("dcround_update groupid.%llu from %llu\n",(long long)group->id,(long long)sender);
    for (i=0; i<group->n; i++)
    {
        if ( (node= group->nodes[i]) != 0 && node->id == sender )
        {
            if ( group->Ois[i].txid == 0 )
            {
                printf("MATCHED ind.%d\n",i);
                group->Ois[i] = Oi, group->commits[i] = commit;
                group->prodOi = fmul(group->prodOi,fexpand(Oi));
                group->prodcommit = fmul(group->prodcommit,fexpand(commit));
                memset(msgstr,0,sizeof(msgstr));
                if ( ++group->nonz >= group->n )
                {
                    if ( group->prodOi.txid != Unit.txid )
                    {
                        msg = fcontract(group->prodOi);
                        desti = (msg.bytes[31] & 0x3f);
                        huff256(&H,&msg);
                        for (i=numbits=0; i<30&&msgstr[i]!=0; i++)
                            msgstr[i] = hread(&numbits,8,&H);
                    } else strcpy(msgstr,"no message");
                    printf("node %llu received prod (Oi.%llx commit.%llx) -> msg.(%s) desti.%d\n",(long long)DCNET.myid,(long long)group->prodOi.txid,(long long)group->prodcommit.txid,msgstr,desti);
                }
            } else printf("DUPLICATE.%d\n",i);
            return;
        }
    }
    printf("dcround_update: couldnt find match\n");
}

void dcnet(char *dccmd,cJSON *json)
{
    int32_t i,j,n,myind,datalen = 0; bits256 pubkey,pubkey2; bits320 pubexp,pubexp2,Oi,commit,msgelement;
    struct dcnode *node; struct dcgroup *group; cJSON *array; uint64_t txid,groupid; char *pubkeystr,*pubkey2str; uint8_t data[8192];
    if ( strcmp(dccmd,"join") == 0 )
    {
        pubkeystr = jstr(json,"pubkey"), pubkey2str = jstr(json,"pubkey2");
        if ( pubkeystr != 0 && pubkey2str != 0 )
        {
            decode_hex(pubkey.bytes,sizeof(pubkey),pubkeystr), pubexp = fexpand(pubkey);
            decode_hex(pubkey2.bytes,sizeof(pubkey2),pubkey2str), pubexp2 = fexpand(pubkey2);
            printf("dcnet.(%s) P.(%s) P2.(%s)\n",dccmd,pubkeystr,pubkey2str);
            for (i=0; i<DCNET.num; i++)
            {
                node = &DCNET.nodes[i];
                if ( pubexp.txid == node->pubexp.txid )
                {
                    printf("duplicate %llx, %d of DCNET.num %d\n",(long long)pubexp.txid,i,DCNET.num);
                    node->lastcontact = (uint32_t)time(NULL);
                    return;
                }
                if ( pubexp.txid > node->pubexp.txid )
                {
                    for (j=DCNET.num; j>i; j--)
                        DCNET.nodes[j] = DCNET.nodes[j-1];
                    break;
                }
            }
            set_dcnode(&DCNET.nodes[i],pubexp,pubexp2);
            DCNET.num++;
            for (i=0; i<DCNET.num; i++)
                printf("%llx.%u ",(long long)DCNET.nodes[i].pubexp.txid,DCNET.nodes[i].lastcontact);
            printf("DCNET.num %d\n",DCNET.num);
        }
    }
    else if ( strcmp(dccmd,"round") == 0 )
    {
        pubkeystr = jstr(json,"G"), pubkey2str = jstr(json,"H");
        if ( pubkeystr != 0 && pubkey2str != 0 && (array= jarray(&n,json,"group")) != 0 && (groupid= j64bits(json,"groupid")) != 0 && n <= MAXNODES && n > 3 )
        {
            if ( find_dcgroup(groupid) != 0 )
            {
                printf("groupid.%llx already exists\n",(long long)groupid);
                return;
            }
            group = &DCNET.groups[DCNET.numgroups++], memset(group,0,sizeof(*group));
            group->id = groupid, group->created = (uint32_t)time(NULL);
            group->prodcommit = group->prodOi = Unit;
            decode_hex(pubkey.bytes,sizeof(pubkey),pubkeystr), pubexp = fexpand(pubkey);
            decode_hex(pubkey2.bytes,sizeof(pubkey2),pubkey2str), pubexp2 = fexpand(pubkey2);
            for (i=0; i<n; i++)
            {
                txid = j64bits(jitem(array,i),0);
                if ( txid == DCNET.myid )
                    myind = i, group->myind = i;
                else if ( (node= find_dcnode(txid)) != 0 )
                    group->nodes[group->n++] = node;
                else
                {
                    printf("cant find array[%d] %llx\n",i,(long long)txid);
                    DCNET.numgroups--;
                    return;
                }
            }
            msgelement = dcnet_message(group,n);
            Oi = dcround(&commit,myind,group,pubkey,pubkey2,msgelement);
            datalen = serialize_data(data,datalen,groupid,sizeof(groupid));
            datalen = serialize_data(data,datalen,DCNET.myid,sizeof(DCNET.myid));
            datalen = serialize_data256(data,datalen,fcontract(Oi));
            datalen = serialize_data256(data,datalen,fcontract(commit));
            printf("dcnet.(%s) G.(%s) H.(%s) -> (%llx, %llx)\n",dccmd,pubkeystr,pubkey2str,(long long)Oi.txid,(long long)commit.txid);
        }
    }
}

int32_t dcnet_idle(struct plugin_info *plugin)
{
    int32_t len,datalen = 0; uint64_t groupid,sender; struct dcgroup *group; char *msg,*jsonstr,*dccmd; cJSON *json; bits256 Oi,commit;
    if ( DCNET.bus >= 0 )
    {
        if ( (len= nn_recv(DCNET.bus,&msg,NN_MSG,0)) > 0 )
        {
            datalen = deserialize_data((void *)msg,0,&groupid,sizeof(groupid));
            printf("RECV.groupid %llu %p len.%d\n",(long long)groupid,find_dcgroup(groupid),len);
            if ( (group= find_dcgroup(groupid)) != 0 )
            {
                if ( len >= sizeof(groupid)+sizeof(sender)+sizeof(Oi)+sizeof(commit) )
                {
                    datalen = deserialize_data((void *)msg,datalen,&sender,sizeof(sender));
                    datalen = deserialize_data256((void *)msg,datalen,&Oi);
                    datalen = deserialize_data256((void *)msg,datalen,&commit);
                    dcround_update(group,sender,Oi,commit,&msg[datalen],len - datalen);
                } else printf("RECV len.%d too small for %ld\n",len,sizeof(groupid)+sizeof(sender)+sizeof(Oi)+sizeof(commit));
                nn_freemsg(msg);
            }
            else
            {
                jsonstr = clonestr(msg);
                nn_freemsg(msg);
                if ( (json= cJSON_Parse(jsonstr)) != 0 )
                {
                    printf("DCRECV.(%s)\n",jsonstr);
                    if ( (dccmd= jstr(json,"dcnet")) != 0 )
                        dcnet(dccmd,json);
                    free_json(json);
                }
                free(jsonstr);
            }
        }
    }
    return(0);
}

int32_t PLUGNAME(_process_json)(char *forwarder,char *sender,int32_t valid,struct plugin_info *plugin,uint64_t tag,char *retbuf,int32_t maxlen,char *jsonstr,cJSON *json,int32_t initflag,char *tokenstr)
{
    char connectaddr[64],pubhex[65],pubhex2[65],*resultstr,*methodstr,*myip,*arraystr,*retstr = 0; bits256 tmp,pubkey,pubkey2; cJSON *array;
    bits320 z,zmone; uint64_t groupid,txid; int32_t i,n,len,sendtimeout = 10,recvtimeout = 1;
    retbuf[0] = 0;
    plugin->allowremote = 1;
    if ( initflag > 0 )
    {
        char *ipaddrs[] = { "5.9.56.103", "5.9.102.210", "89.248.160.237", "89.248.160.238", "89.248.160.239", "89.248.160.240", "89.248.160.241", "89.248.160.242" };
        fprintf(stderr,"<<<<<<<<<<<< INSIDE PLUGIN! process %s (%s)\n",plugin->name,jsonstr);
        for (i=0; i<1000; i++)
        {
            tmp = rand256(1);
            curve25519_jl777(tmp,rand256(0));
        }
        z = fexpand(tmp);
        zmone = crecip(z);
        Unit = fmul(z,zmone);
        dcinit();
        DCNET.bus = -1;
        DCNET.mode = juint(json,"dchost");
        myip = jstr(json,"myipaddr");
        if ( DCNET.mode != 0 )
        {
            if ( DCNET.mode > 1 && myip != 0 )
            {
                safecopy(plugin->ipaddr,myip,sizeof(plugin->ipaddr));
                if ( (DCNET.bus= nn_socket(AF_SP,NN_BUS)) != 0 )
                {
                    sprintf(DCNET.bind,"tcp://%s:9999",myip);
                    if ( nn_bind(DCNET.bus,DCNET.bind) >= 0 )
                    {
                        if ( nn_settimeouts2(DCNET.bus,sendtimeout,recvtimeout) != 0 )
                            printf("error setting timeouts\n");
                        else printf("DCBIND.(%s) -> NN_BUS\n",DCNET.bind);
                    } else printf("error with DCBIND.(%s)\n",DCNET.bind);
                }
            } else strcpy(plugin->ipaddr,"127.0.0.1");
            if ( DCNET.bus < 0 && (DCNET.bus= nn_socket(AF_SP,NN_BUS)) != 0 )
            {
                if ( nn_settimeouts2(DCNET.bus,sendtimeout,recvtimeout) != 0 )
                    printf("error setting timeouts\n");
            }
            if ( DCNET.bus >= 0 )
            {
                for (i=0; i<sizeof(ipaddrs)/sizeof(*ipaddrs); i++)
                {
                    if ( strcmp(ipaddrs[i],myip) != 0 )
                    {
                        sprintf(connectaddr,"tcp://%s:9999",ipaddrs[i]);
                        if ( nn_connect(DCNET.bus,connectaddr) >= 0 )
                            printf("+%s ",connectaddr);
                    }
                }
            }
        }
        printf("ipaddr.%s bindaddr.%s\n",plugin->ipaddr,DCNET.bind);
        strcpy(retbuf,"{\"result\":\"dcnet init\"}");
    }
    else
    {
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
        else if ( strcmp(methodstr,"join") == 0 )
        {
            pubkey = fcontract(DCNET.pubexp), pubkey2 = fcontract(DCNET.pubexp2);
            init_hexbytes_noT(pubhex,pubkey.bytes,sizeof(pubkey));
            init_hexbytes_noT(pubhex2,pubkey2.bytes,sizeof(pubkey2));
            sprintf(retbuf,"{\"result\":\"success\",\"dcnet\":\"join\",\"pubkey\":\"%s\",\"pubkey2\":\"%s\",\"done\":1,\"dcnum\":%d}",pubhex,pubhex2,DCNET.num);
            len = (int32_t)strlen(retbuf) + 1;
            if ( DCNET.mode > 0 && DCNET.bus >= 0 )
                nn_send(DCNET.bus,retbuf,len,0);
            return(len);
        }
        else if ( strcmp(methodstr,"round") == 0 )
        {
            pubkey = rand256(1), pubkey2 = rand256(1);
            init_hexbytes_noT(pubhex,pubkey.bytes,sizeof(pubkey));
            init_hexbytes_noT(pubhex2,pubkey2.bytes,sizeof(pubkey2));
            array = cJSON_CreateArray();
            for (groupid=i=n=0; i<DCNET.num; i++)
                if ( (time(NULL) - DCNET.nodes[i].lastcontact) < 333 )
                {
                    txid = fcontract(DCNET.nodes[i].pubexp).txid;
                    jaddi64bits(array,txid);
                    groupid ^= txid;
                    n++;
                }
            arraystr = jprint(array,1);
            sprintf(retbuf,"{\"result\":\"success\",\"dcnet\":\"round\",\"G\":\"%s\",\"H\":\"%s\",\"done\":1,\"dcnum\":%d,\"groupid\":\"%llu\",\"group\":%s}",pubhex,pubhex2,DCNET.num,(long long)groupid,arraystr);
            len = (int32_t)strlen(retbuf) + 1;
            if ( DCNET.mode > 0 && DCNET.bus >= 0 )
                nn_send(DCNET.bus,retbuf,len,0);
            return(len);
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
#include "../plugin777.c"

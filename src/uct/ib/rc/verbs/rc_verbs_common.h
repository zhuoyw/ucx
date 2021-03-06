/**
* Copyright (C) Mellanox Technologies Ltd. 2001-2014.  ALL RIGHTS RESERVED.
*
* See file LICENSE for terms.
*/

#ifndef UCT_RC_VERBS_COMMON_H
#define UCT_RC_VERBS_COMMON_H

#include <ucs/arch/bitops.h>
#include <ucs/datastruct/ptr_array.h>

#include <uct/ib/rc/base/rc_iface.h>
#include <uct/ib/rc/base/rc_ep.h>


/* definitions common to rc_verbs and dc_verbs go here */

#define UCT_RC_VERBS_CHECK_AM_SHORT(_iface, _id, _length) \
     UCT_CHECK_AM_ID(_id); \
     UCT_CHECK_LENGTH(sizeof(uct_rc_am_short_hdr_t) + _length + \
                      (_iface)->config.notag_hdr_size, 0, \
                      (_iface)->config.max_inline, "am_short");

#define UCT_RC_VERBS_CHECK_AM_ZCOPY(_iface, _id, _header_len, _len, _seg_size) \
     UCT_CHECK_AM_ID(_id); \
     UCT_RC_CHECK_ZCOPY_DATA(_header_len, _len, _seg_size) \
     UCT_CHECK_LENGTH(sizeof(uct_rc_hdr_t) + _header_len + \
                      (_iface)->config.notag_hdr_size, 0, \
                      (_iface)->config.short_desc_size, "am_zcopy header");

#define UCT_RC_VERBS_GET_TX_DESC(_iface, _rc_iface, _mp, _desc, _hdr, _len) \
     { \
         UCT_RC_IFACE_GET_TX_DESC(_rc_iface, _mp, _desc) \
         hdr = _desc + 1; \
         len = uct_rc_verbs_notag_header_fill(_iface, _hdr); \
     }

#define UCT_RC_VERBS_GET_TX_AM_BCOPY_DESC(_iface, _rc_iface, _mp, _desc, _id, \
                                          _pack_cb, _arg, _length, \
                                          _data_length) \
     { \
         void *hdr; \
         size_t len; \
         UCT_RC_VERBS_GET_TX_DESC(_iface, _rc_iface,_mp, _desc, hdr, len) \
         (_desc)->super.handler = (uct_rc_send_handler_t)ucs_mpool_put; \
         uct_rc_bcopy_desc_fill(hdr + len, _id, _pack_cb, \
                               _arg, &(_data_length)); \
         _length = _data_length + len + sizeof(uct_rc_hdr_t); \
     }

#define UCT_RC_VERBS_GET_TX_AM_ZCOPY_DESC(_iface, _rc_iface, _mp, _desc, _id, \
                                          _header,  _header_length, _comp, \
                                          _send_flags, _sge) \
     { \
         void *hdr; \
         size_t len; \
         UCT_RC_VERBS_GET_TX_DESC(_iface, _rc_iface, _mp, _desc, hdr, len) \
         uct_rc_zcopy_desc_set_comp(_desc, _comp, _send_flags); \
         uct_rc_zcopy_desc_set_header(hdr + len, _id, _header, _header_length); \
         _sge.length = sizeof(uct_rc_hdr_t) + header_length + len; \
     }

#define UCT_RC_VERBS_IFACE_FOREACH_TXWQE(_iface, _i, _wc, _num_wcs) \
      status = uct_ib_poll_cq((_iface)->super.send_cq, &_num_wcs, _wc); \
      if (status != UCS_OK) { \
          return 0; \
      } \
      UCS_STATS_UPDATE_COUNTER((_iface)->stats, \
                               UCT_RC_IFACE_STAT_TX_COMPLETION, _num_wcs); \
      for (_i = 0; _i < _num_wcs; ++_i)


typedef struct uct_rc_verbs_txcnt {
    uint16_t       pi;      /* producer (post_send) count */
    uint16_t       ci;      /* consumer (ibv_poll_cq) completion count */
} uct_rc_verbs_txcnt_t;


#if IBV_EXP_HW_TM
typedef struct uct_rc_verbs_release_desc {
    uct_recv_desc_t             super;
    unsigned                    offset;
} uct_rc_verbs_release_desc_t;

typedef struct uct_rc_verbs_ctx_priv {
    uint64_t                    tag;
    uint64_t                    imm_data;
    void                        *buffer;
    uint32_t                    length;
    uint32_t                    tag_handle;
} uct_rc_verbs_ctx_priv_t;
#endif


/**
 * RC/DC verbs interface configuration
 */
typedef struct uct_rc_verbs_iface_common_config {
    size_t                 max_am_hdr;
    unsigned               tx_max_wr;
#if IBV_EXP_HW_TM
    struct {
        int                            enable;
        unsigned                       list_size;
        unsigned                       rndv_queue_len;
        double                         sync_ratio;
    } tm;
#endif
    /* TODO flags for exp APIs */
} uct_rc_verbs_iface_common_config_t;


typedef struct uct_rc_verbs_iface_common {
    struct ibv_sge         inl_sge[2];
    void                   *am_inl_hdr;
    ucs_mpool_t            short_desc_mp;
#if IBV_EXP_HW_TM
    struct {
        uct_rc_srq_t            xrq;       /* TM XRQ */
        ucs_ptr_array_t         rndv_comps;
        unsigned                num_tags;
        unsigned                num_outstanding;
        unsigned                num_canceled;
        unsigned                tag_sync_thresh;
        uint16_t                unexpected_cnt;
        uint8_t                 enabled;
        struct {
            void                     *arg; /* User defined arg */
            uct_tag_unexp_eager_cb_t cb;   /* Callback for unexpected eager messages */
        } eager_unexp;

        struct {
            void                     *arg; /* User defined arg */
            uct_tag_unexp_rndv_cb_t  cb;   /* Callback for unexpected rndv messages */
        } rndv_unexp;
        uct_rc_verbs_release_desc_t  eager_desc;
        uct_rc_verbs_release_desc_t  rndv_desc;
    } tm;
#endif
    /* TODO: make a separate datatype */
    struct {
        size_t             notag_hdr_size;
        size_t             short_desc_size;
        size_t             max_inline;
    } config;
} uct_rc_verbs_iface_common_t;


extern ucs_config_field_t uct_rc_verbs_iface_common_config_table[];

void uct_rc_verbs_txcnt_init(uct_rc_verbs_txcnt_t *txcnt);

static inline void
uct_rc_verbs_txqp_posted(uct_rc_txqp_t *txqp, uct_rc_verbs_txcnt_t *txcnt,
                         uct_rc_iface_t *iface, int signaled)
{
    txcnt->pi++;
    uct_rc_txqp_posted(txqp, iface, 1, signaled);
}

static inline void
uct_rc_verbs_txqp_completed(uct_rc_txqp_t *txqp, uct_rc_verbs_txcnt_t *txcnt, uint16_t count)
{
    txcnt->ci += count;
    uct_rc_txqp_available_add(txqp, count);
}

void uct_rc_verbs_iface_common_preinit(uct_rc_verbs_iface_common_t *iface,
                                       uct_md_h md,
                                       uct_rc_iface_config_t *rc_config,
                                       uct_rc_verbs_iface_common_config_t *config,
                                       const uct_iface_params_t *params,
                                       int is_dc, unsigned *rx_cq_len,
                                       unsigned *srq_size,
                                       unsigned *rx_hdr_len,
                                       unsigned *short_mp_size);

ucs_status_t uct_rc_verbs_iface_common_init(uct_rc_verbs_iface_common_t *iface,
                                            uct_rc_iface_t *rc_iface,
                                            uct_rc_verbs_iface_common_config_t *config,
                                            uct_rc_iface_config_t *rc_config,
                                            unsigned short_mp_size);

void uct_rc_verbs_iface_common_cleanup(uct_rc_verbs_iface_common_t *iface);

ucs_status_t uct_rc_verbs_iface_prepost_recvs_common(uct_rc_iface_t *iface,
                                                     uct_rc_srq_t *srq);

void uct_rc_verbs_iface_common_query(uct_rc_verbs_iface_common_t *verbs_iface,
                                     uct_rc_iface_t *rc_iface, uct_iface_attr_t *iface_attr);

unsigned uct_rc_verbs_iface_post_recv_always(uct_rc_iface_t *iface,
                                             uct_rc_srq_t *srq, unsigned max);

static inline unsigned uct_rc_verbs_iface_post_recv_common(uct_rc_iface_t *iface,
                                                           uct_rc_srq_t *srq,
                                                           int fill)
{
    unsigned batch = iface->super.config.rx_max_batch;
    unsigned count;

    if (srq->available < batch) {
        if (ucs_likely(fill == 0)) {
            return 0;
        } else {
            count = srq->available;
        }
    } else {
        count = batch;
    }
    return uct_rc_verbs_iface_post_recv_always(iface, srq, count);
}


/* TODO: think of a better name */
static inline int
uct_rc_verbs_txcq_get_comp_count(struct ibv_wc *wc, uct_rc_txqp_t *txqp)
{
    uint16_t count = 1;

    if (ucs_likely(wc->wr_id != RC_UNSIGNALED_INF)) {
        return wc->wr_id + 1;
    }

    ucs_assert(txqp->unsignaled_store != RC_UNSIGNALED_INF);
    ucs_assert(txqp->unsignaled_store_count != 0);

    txqp->unsignaled_store_count--;
    if (txqp->unsignaled_store_count == 0) {
        count += txqp->unsignaled_store;
        txqp->unsignaled_store = 0;
    }

    return count;
}

static UCS_F_ALWAYS_INLINE void
uct_rc_verbs_iface_handle_am(uct_rc_iface_t *iface, uct_rc_hdr_t *hdr,
                             uint64_t wr_id, uint32_t qp_num, uint32_t length,
                             uint32_t imm_data, uint32_t slid)
{
    uct_ib_iface_recv_desc_t *desc;
    uct_rc_iface_ops_t *rc_ops;
    void *udesc;
    ucs_status_t status;

    desc = (uct_ib_iface_recv_desc_t *)wr_id;
    if (ucs_unlikely(hdr->am_id & UCT_RC_EP_FC_MASK)) {
        rc_ops = ucs_derived_of(iface->super.ops, uct_rc_iface_ops_t);
        status = rc_ops->fc_handler(iface, qp_num, hdr, length - sizeof(*hdr),
                                    imm_data, slid, UCT_CB_PARAM_FLAG_DESC);
        if (status == UCS_OK) {
            ucs_mpool_put_inline(desc);
        } else {
            udesc = (char*)desc + iface->super.config.rx_headroom_offset;
            uct_recv_desc(udesc) = &iface->super.release_desc;
        }
    } else {
        uct_ib_iface_invoke_am_desc(&iface->super, hdr->am_id, hdr + 1,
                                    length - sizeof(*hdr), desc);
    }
}

static UCS_F_ALWAYS_INLINE unsigned
uct_rc_verbs_iface_poll_rx_common(uct_rc_iface_t *iface)
{
    uct_rc_hdr_t *hdr;
    unsigned i;
    ucs_status_t status;
    unsigned num_wcs = iface->super.config.rx_max_poll;
    struct ibv_wc wc[num_wcs];

    status = uct_ib_poll_cq(iface->super.recv_cq, &num_wcs, wc);
    if (status != UCS_OK) {
        num_wcs = 0;
        goto out;
    }

    UCT_IB_IFACE_VERBS_FOREACH_RXWQE(&iface->super, i, hdr, wc, num_wcs) {
        uct_ib_log_recv_completion(&iface->super, IBV_QPT_RC, &wc[i], hdr,
                                   wc[i].byte_len, uct_rc_ep_am_packet_dump);
        uct_rc_verbs_iface_handle_am(iface, hdr, wc[i].wr_id, wc[i].qp_num,
                                     wc[i].byte_len, wc[i].imm_data, wc[i].slid);
    }
    iface->rx.srq.available += num_wcs;
    UCS_STATS_UPDATE_COUNTER(iface->stats, UCT_RC_IFACE_STAT_RX_COMPLETION, num_wcs);

out:
    uct_rc_verbs_iface_post_recv_common(iface, &iface->rx.srq, 0);
    return num_wcs;
}

static UCS_F_ALWAYS_INLINE void
uct_rc_verbs_iface_fill_inl_sge(uct_rc_verbs_iface_common_t *iface, const void *addr0,
                                unsigned len0, const void* addr1, unsigned len1)
{
    iface->inl_sge[0].addr      = (uintptr_t)addr0;
    iface->inl_sge[0].length    = len0;
    iface->inl_sge[1].addr      = (uintptr_t)addr1;
    iface->inl_sge[1].length    = len1;
}

static inline void
uct_rc_verbs_iface_fill_inl_am_sge(uct_rc_verbs_iface_common_t *iface,
                                   uint8_t id, uint64_t hdr,
                                   const void *buffer, unsigned length)
{
    uct_rc_am_short_hdr_t *am = (uct_rc_am_short_hdr_t*)((char*)iface->am_inl_hdr +
                                iface->config.notag_hdr_size);
    am->rc_hdr.am_id = id;
    am->am_hdr       = hdr;
    uct_rc_verbs_iface_fill_inl_sge(iface, iface->am_inl_hdr,
                                    sizeof(*am) + iface->config.notag_hdr_size,
                                    buffer, length);
}

#if IBV_EXP_HW_TM

#  define UCT_RC_VERBS_TAG_MIN_POSTED  33

/* If message arrived with imm_data = 0 - it is SW RNDV request */
#  define UCT_RC_VERBS_TM_IS_SW_RNDV(_flags, _imm_data) \
       (ucs_unlikely(((_flags) & IBV_EXP_WC_WITH_IMM) && !(_imm_data)))

#  define UCT_RC_VERBS_GET_TX_TM_DESC(_iface, _mp, _desc, _tag, _app_ctx, _hdr) \
       { \
           UCT_RC_IFACE_GET_TX_DESC(_iface, _mp, _desc) \
           hdr = _desc + 1; \
           uct_rc_verbs_iface_fill_tmh(_hdr, _tag, _app_ctx, IBV_EXP_TMH_EAGER); \
           hdr += sizeof(struct ibv_exp_tmh); \
       }

#  define UCT_RC_VERBS_GET_TM_BCOPY_DESC(_iface, _mp, _desc, _tag, _app_ctx, \
                                         _pack_cb, _arg, _length) \
       { \
           void *hdr; \
           UCT_RC_VERBS_GET_TX_TM_DESC(_iface, _mp, _desc, _tag, _app_ctx, hdr) \
           (_desc)->super.handler = (uct_rc_send_handler_t)ucs_mpool_put; \
           _length = pack_cb(hdr, arg); \
       }

#  define UCT_RC_VERBS_GET_TM_ZCOPY_DESC(_iface, _mp, _desc, _tag, _app_ctx, \
                                         _comp, _send_flags, _sge) \
       { \
           void *hdr; \
           UCT_RC_VERBS_GET_TX_TM_DESC(_iface, _mp, _desc, _tag, _app_ctx, hdr) \
           uct_rc_zcopy_desc_set_comp(_desc, _comp, _send_flags); \
           _sge.length = sizeof(struct ibv_exp_tmh); \
       }

#  define UCT_RC_VERBS_FILL_TM_IMM(_wr, _imm_data, _priv) \
       if (_imm_data == 0) { \
           _wr.opcode = IBV_WR_SEND; \
           _priv = 0; \
       } else { \
           _wr.opcode = IBV_WR_SEND_WITH_IMM; \
           uct_rc_verbs_tag_imm_data_pack(&(_wr.imm_data), &_priv, _imm_data); \
       }

#  define UCT_RC_VERBS_FILL_TM_ADD_WR(_wr, _tag, _tag_mask, _sge, _sge_cnt, _ctx) \
       { \
           (_wr)->tm.add.tag        = tag; \
           (_wr)->tm.add.mask       = tag_mask; \
           (_wr)->tm.add.sg_list    = _sge; \
           (_wr)->tm.add.num_sge    = _sge_cnt; \
           (_wr)->tm.add.recv_wr_id = (uint64_t)_ctx; \
       }

#  define UCT_RC_VERBS_FILL_TM_OP_WR(_iface, _wr, _opcode, _flags, _wr_id) \
       { \
           (_wr)->tm.unexpected_cnt = (_iface)->tm.unexpected_cnt; \
           (_wr)->wr_id             = _wr_id; \
           (_wr)->opcode            = (enum ibv_exp_ops_wr_opcode)_opcode; \
           (_wr)->flags             = _flags | IBV_EXP_OPS_TM_SYNC; \
           (_wr)->next              = NULL; \
       }

#  define UCT_RC_VERBS_CHECK_TAG(_iface) \
       if (!(_iface)->tm.num_tags) {  \
           return UCS_ERR_EXCEEDS_LIMIT; \
       }


static UCS_F_ALWAYS_INLINE void
uct_rc_verbs_iface_fill_tmh(struct ibv_exp_tmh *tmh, uct_tag_t tag,
                            uint32_t app_ctx, unsigned op)
{
    tmh->opcode  = op;
    tmh->app_ctx = htonl(app_ctx);
    tmh->tag     = htobe64(tag);
}

static UCS_F_ALWAYS_INLINE void
uct_rc_verbs_iface_fill_rvh(struct ibv_exp_tmh_rvh *rvh, const void *vaddr,
                            uint32_t rkey, uint32_t len)
{
    rvh->va   = htobe64((uint64_t)vaddr);
    rvh->rkey = htonl(rkey);
    rvh->len  = htonl(len);
}

static UCS_F_ALWAYS_INLINE void
uct_rc_verbs_tag_imm_data_pack(uint32_t *ib_imm, uint32_t *app_ctx,
                               uint64_t imm_val)
{
    *ib_imm  = (uint32_t)(imm_val & 0xFFFFFFFF);
    *app_ctx = (uint32_t)(imm_val >> 32);
}

static UCS_F_ALWAYS_INLINE uint64_t
uct_rc_verbs_tag_imm_data_unpack(struct ibv_exp_wc *wc, uint32_t app_ctx)
{
    if (wc->exp_wc_flags & IBV_EXP_WC_WITH_IMM) {
        return ((uint64_t)app_ctx << 32) | wc->imm_data;
    } else {
        return 0ul;
    }
}

static UCS_F_ALWAYS_INLINE uct_rc_verbs_ctx_priv_t*
uct_rc_verbs_iface_ctx_priv(uct_tag_context_t *ctx)
{
    return (uct_rc_verbs_ctx_priv_t*)ctx->priv;
}

static UCS_F_ALWAYS_INLINE unsigned
uct_rc_verbs_iface_tag_get_op_id(uct_rc_verbs_iface_common_t *iface,
                                 uct_completion_t *comp)
{
    uint32_t prev_ph;
    return ucs_ptr_array_insert(&iface->tm.rndv_comps, comp, &prev_ph);
}

static UCS_F_ALWAYS_INLINE ucs_status_t
uct_rc_verbs_iface_post_op(uct_rc_verbs_iface_common_t *iface,
                           struct ibv_exp_ops_wr *wr, int op,
                           int flags, uint64_t wr_id)
{
    struct ibv_exp_ops_wr *bad_wr;
    int ret;

    UCT_RC_VERBS_FILL_TM_OP_WR(iface, wr, op, flags, wr_id);

    ret = ibv_exp_post_srq_ops(iface->tm.xrq.srq, wr, &bad_wr);
    if (ret) {
        ucs_error("ibv_exp_post_srq_ops(op=%d) failed: %m", op);
        return UCS_ERR_IO_ERROR;
    }
    return UCS_OK;
}

static UCS_F_ALWAYS_INLINE ucs_status_t
uct_rc_verbs_iface_post_signaled_op(uct_rc_verbs_iface_common_t *iface,
                                    struct ibv_exp_ops_wr *wr, int op)
{
    ucs_status_t status;

    status = uct_rc_verbs_iface_post_op(iface, wr, op, IBV_EXP_OPS_SIGNALED,
                                        iface->tm.num_canceled);
    if (status != UCS_OK) {
        return status;
    }

    iface->tm.num_canceled = 0;
    return UCS_OK;
}

static UCS_F_ALWAYS_INLINE ucs_status_t
uct_rc_verbs_iface_common_tag_recv(uct_rc_verbs_iface_common_t *iface,
                                   uct_tag_t tag, uct_tag_t tag_mask,
                                   const uct_iov_t *iov, size_t iovcnt,
                                   uct_tag_context_t *ctx)
{
    uct_rc_verbs_ctx_priv_t *priv = (uct_rc_verbs_ctx_priv_t*)ctx->priv;
    ucs_status_t status;
    struct ibv_sge sge[UCT_IB_MAX_IOV];
    struct ibv_exp_ops_wr wr;
    size_t sge_cnt;

    UCT_CHECK_IOV_SIZE(iovcnt, 1ul, "uct_rc_verbs_iface_tag_recv_zcopy");
    UCT_RC_VERBS_CHECK_TAG(iface);

    sge_cnt = uct_ib_verbs_sge_fill_iov(sge, iov, iovcnt);
    UCT_RC_VERBS_FILL_TM_ADD_WR(&wr, tag, tag_mask, sge, sge_cnt, ctx);

    status = uct_rc_verbs_iface_post_signaled_op(iface, &wr,
                                                 IBV_EXP_WR_TAG_ADD);
    if (ucs_unlikely(status != UCS_OK)) {
        return status;
    }

    --iface->tm.num_tags;
    ++iface->tm.num_outstanding;

    /* Save tag index in the device tags list returned by ibv_exp_post_srq_ops.
     * It may be needed for cancelling this posted tag. */
    priv->tag_handle = wr.tm.handle;
    priv->tag        = tag;
    priv->buffer     = iov->buffer; /* Only one iov is supported so far */
    priv->length     = uct_iov_total_length(iov, iovcnt);
    return UCS_OK;
}

static UCS_F_ALWAYS_INLINE ucs_status_t
uct_rc_verbs_iface_common_tag_recv_cancel(uct_rc_verbs_iface_common_t *iface,
                                          uct_tag_context_t *ctx, int force)
{
   uct_rc_verbs_ctx_priv_t *priv = (uct_rc_verbs_ctx_priv_t*)ctx->priv;
   struct ibv_exp_ops_wr wr;
   ucs_status_t status;

   wr.tm.handle = priv->tag_handle;

   status = uct_rc_verbs_iface_post_op(iface, &wr, IBV_EXP_WR_TAG_DEL,
                                       force ? 0 : IBV_EXP_OPS_SIGNALED,
                                       (uint64_t)ctx);
   if (status != UCS_OK) {
       return status;
   }

   if (force) {
       if (iface->tm.num_outstanding) {
           ++iface->tm.num_canceled;
       } else {
           /* No pending ADD operations, free the tag immediately */
           ++iface->tm.num_tags;
       }
       if (iface->tm.num_canceled > iface->tm.tag_sync_thresh) {
           /* Too many pending cancels. Need to issue a signaled operation
            * to free the canceled tags */
           uct_rc_verbs_iface_post_signaled_op(iface, &wr, IBV_EXP_WR_TAG_SYNC);
       }
   }

   return UCS_OK;
}

/* This function check whether the error occured due to "MESSAGE_TRUNCATED"
 * error in Tag Matching (i.e. if posted buffer was not enough to fit the
 * incoming message). If this is the case the error should be reported in
 * the corresponding callback and QP should be reset back to normal. Otherwise
 * treat the error as fatal. */
static UCS_F_NOINLINE void
uct_rc_verbs_iface_wc_error(enum ibv_wc_status status)
{
    /* TODO: handle MSG TRUNCATED error */
    ucs_fatal("Receive completion with error on XRQ: %s",
              ibv_wc_status_str(status));
}

static UCS_F_ALWAYS_INLINE void
uct_rc_verbs_iface_tag_handle_exp(uct_rc_verbs_iface_common_t *iface,
                                  struct ibv_exp_wc *wc)
{
    uct_tag_context_t *ctx        = (uct_tag_context_t*)wc->wr_id;
    uct_rc_verbs_ctx_priv_t *priv = uct_rc_verbs_iface_ctx_priv(ctx);

    if (wc->exp_wc_flags & IBV_EXP_WC_TM_MATCH) {
        /* Need to keep app_ctx in case DATA will come with immediate */
        priv->imm_data = wc->tm_info.priv;
        priv->tag      = wc->tm_info.tag;
        ctx->tag_consumed_cb(ctx);
    }

    if (wc->exp_wc_flags & IBV_EXP_WC_TM_DATA_VALID) {
        priv->imm_data = uct_rc_verbs_tag_imm_data_unpack(wc, priv->imm_data);
        if (UCT_RC_VERBS_TM_IS_SW_RNDV(wc->exp_wc_flags, priv->imm_data)) {
            ctx->rndv_cb(ctx, priv->tag, priv->buffer, wc->byte_len, UCS_OK);
        } else {
            ctx->completed_cb(ctx, priv->tag, priv->imm_data,
                              wc->byte_len, UCS_OK);
        }
        ++iface->tm.num_tags;
    }
}

static UCS_F_ALWAYS_INLINE void
uct_rc_verbs_iface_unexp_consumed(uct_rc_verbs_iface_common_t *iface,
                                  uct_ib_iface_recv_desc_t *ib_desc,
                                  uct_rc_verbs_release_desc_t *release,
                                  ucs_status_t comp_status)
{
    struct ibv_exp_ops_wr wr;
    void *udesc;

    if (comp_status == UCS_OK) {
        ucs_mpool_put_inline(ib_desc);
    } else {
        udesc = (char*)ib_desc + release->offset;
        uct_recv_desc(udesc) = &release->super;
    }

    if (ucs_unlikely(!(++iface->tm.unexpected_cnt % IBV_DEVICE_MAX_UNEXP_COUNT))) {
        uct_rc_verbs_iface_post_signaled_op(iface, &wr, IBV_EXP_WR_TAG_SYNC);
    }
    ++iface->tm.xrq.available;
}

static UCS_F_ALWAYS_INLINE void
uct_rc_verbs_iface_tag_handle_unexp(uct_rc_verbs_iface_common_t *iface,
                                    uct_rc_iface_t *rc_iface,
                                    struct ibv_exp_wc *wc)
{
    uct_ib_md_t *ib_md = uct_ib_iface_md(&rc_iface->super);
    uct_ib_iface_recv_desc_t *ib_desc = (uct_ib_iface_recv_desc_t*)(uintptr_t)wc->wr_id;
    struct ibv_exp_tmh *tmh;
    uct_rc_hdr_t *rc_hdr;
    uint64_t imm_data;
    ucs_status_t status;
    void *rb;
    int found;
    void *rndv_comp;
    struct ibv_exp_tmh_rvh *rvh;

    tmh = (struct ibv_exp_tmh*)uct_ib_iface_recv_desc_hdr(&rc_iface->super, ib_desc);
    VALGRIND_MAKE_MEM_DEFINED(tmh, wc->byte_len);

    switch (tmh->opcode) {
    case IBV_EXP_TMH_EAGER:
        imm_data = uct_rc_verbs_tag_imm_data_unpack(wc, ntohl(tmh->app_ctx));

        if (UCT_RC_VERBS_TM_IS_SW_RNDV(wc->exp_wc_flags, imm_data)) {
            status = iface->tm.rndv_unexp.cb(iface->tm.rndv_unexp.arg,
                                             UCT_CB_PARAM_FLAG_DESC,
                                             be64toh(tmh->tag), tmh + 1,
                                             wc->byte_len - sizeof(*tmh),
                                             0ul, 0, NULL);
        } else {
            status = iface->tm.eager_unexp.cb(iface->tm.eager_unexp.arg,
                                              tmh + 1, wc->byte_len - sizeof(*tmh),
                                              UCT_CB_PARAM_FLAG_DESC,
                                              be64toh(tmh->tag), imm_data);
        }
        uct_rc_verbs_iface_unexp_consumed(iface, ib_desc, &iface->tm.eager_desc,
                                          status);
        break;

    case IBV_EXP_TMH_NO_TAG:
        rc_hdr = (uct_rc_hdr_t*)((char*)tmh + iface->config.notag_hdr_size);
        uct_ib_log_recv_completion(&rc_iface->super, IBV_QPT_RC, wc, rc_hdr,
                                   wc->byte_len - iface->config.notag_hdr_size,
                                   uct_rc_ep_am_packet_dump);
        uct_rc_verbs_iface_handle_am(rc_iface, rc_hdr, wc->wr_id, wc->qp_num,
                                     wc->byte_len - iface->config.notag_hdr_size,
                                     wc->imm_data, wc->slid);
        ++iface->tm.xrq.available;
        break;

    case IBV_EXP_TMH_RNDV:
        rvh = (struct ibv_exp_tmh_rvh*)(tmh + 1);
        /* Create "packed" rkey to pass it in the callback */
        rb = uct_md_fill_md_name(&ib_md->super, (char*)tmh + wc->byte_len);
        uct_ib_md_pack_rkey(ntohl(rvh->rkey), UCT_IB_INVALID_RKEY, rb);

        status = iface->tm.rndv_unexp.cb(iface->tm.rndv_unexp.arg,
                                         UCT_CB_PARAM_FLAG_DESC,
                                         be64toh(tmh->tag), rvh + 1, wc->byte_len -
                                         (sizeof(*tmh) + sizeof(*rvh)),
                                         be64toh(rvh->va), ntohl(rvh->len),
                                         (char*)tmh + wc->byte_len);

        uct_rc_verbs_iface_unexp_consumed(iface, ib_desc,
                                          &iface->tm.rndv_desc, status);
        break;

    case IBV_EXP_TMH_FIN:
        found = ucs_ptr_array_lookup(&iface->tm.rndv_comps, ntohl(tmh->app_ctx),
                                     rndv_comp);
        ucs_assert_always(found > 0);
        uct_invoke_completion((uct_completion_t*)rndv_comp, UCS_OK);
        ucs_ptr_array_remove(&iface->tm.rndv_comps, ntohl(tmh->app_ctx), 0);
        ucs_mpool_put_inline(ib_desc);
        ++rc_iface->rx.srq.available;
        break;

    default:
        ucs_fatal("Unsupported packet arrived %d", tmh->opcode);
        break;
    }
}

static UCS_F_ALWAYS_INLINE unsigned
uct_rc_verbs_iface_poll_rx_tm(uct_rc_verbs_iface_common_t *iface,
                              uct_rc_iface_t *rc_iface)
{
    const unsigned max_wcs = rc_iface->super.config.rx_max_poll;
    struct ibv_exp_wc wc[max_wcs];
    uct_tag_context_t *ctx;
    uct_rc_verbs_ctx_priv_t *priv;
    int num_wcs, i;

    num_wcs = ibv_exp_poll_cq(rc_iface->super.recv_cq, max_wcs, wc,
                              sizeof(wc[0]));
    if (num_wcs <= 0) {
        if (ucs_unlikely(num_wcs < 0)) {
            ucs_fatal("Failed to poll receive CQ %d", num_wcs);
        }
        goto out;
    }

    for (i = 0; i < num_wcs; ++i) {
        if (ucs_unlikely(wc[i].status != IBV_WC_SUCCESS)) {
            uct_rc_verbs_iface_wc_error(wc[i].status);
            continue;
        }

        switch (wc[i].exp_opcode) {
        case IBV_EXP_WC_TM_NO_TAG:
        case IBV_EXP_WC_RECV:
            uct_rc_verbs_iface_tag_handle_unexp(iface, rc_iface, &wc[i]);
            break;

        case IBV_EXP_WC_TM_RECV:
            if (wc[i].exp_wc_flags &
                (IBV_EXP_WC_TM_MATCH | IBV_EXP_WC_TM_DATA_VALID)) {
                uct_rc_verbs_iface_tag_handle_exp(iface, &wc[i]);
            } else {
                uct_rc_verbs_iface_tag_handle_unexp(iface, rc_iface, &wc[i]);
            }
            break;

        case IBV_EXP_WC_TM_DEL:
            ctx  = (uct_tag_context_t*)wc[i].wr_id;
            priv = uct_rc_verbs_iface_ctx_priv(ctx);
            ctx->completed_cb(ctx, priv->tag, 0, priv->length,
                              UCS_ERR_CANCELED);
            ++iface->tm.num_tags;
            break;

        case IBV_EXP_WC_TM_ADD:
            --iface->tm.num_outstanding;
            /* Fall through */
        case IBV_EXP_WC_TM_SYNC:
            iface->tm.num_tags += wc[i].wr_id;
            break;

        default:
            ucs_error("Wrong opcode in CQE %d", wc[i].exp_opcode);
            break;

        }
    }
    /* TODO: Add stat */
out:
    /* All tag unexpected and AM messages arrive to XRQ */
    uct_rc_verbs_iface_post_recv_common(rc_iface, &iface->tm.xrq, 0);

    /* Only RNDV FIN messages arrive to SRQ (sent by FW) */
    uct_rc_verbs_iface_post_recv_common(rc_iface, &rc_iface->rx.srq, 0);
    return num_wcs;
}

#endif /* IBV_EXP_HW_TM */

static UCS_F_ALWAYS_INLINE unsigned
uct_rc_verbs_notag_header_fill(uct_rc_verbs_iface_common_t *iface, void *hdr)
{
#if IBV_EXP_HW_TM
    if (iface->tm.enabled) {
        struct ibv_exp_tmh tmh;

        *(typeof(tmh.opcode)*)hdr = IBV_EXP_TMH_NO_TAG;
        return sizeof(tmh.opcode);
    }
#endif
    return 0;
}

#define UCT_RC_VERBS_FILL_SGE(_wr, _sge, _length) \
    _wr.sg_list = &_sge; \
    _wr.num_sge = 1; \
    _sge.length = _length;

#define UCT_RC_VERBS_FILL_INL_PUT_WR(_iface, _raddr, _rkey, _buf, _len) \
    _iface->inl_rwrite_wr.wr.rdma.remote_addr = _raddr; \
    _iface->inl_rwrite_wr.wr.rdma.rkey        = uct_ib_md_direct_rkey(_rkey); \
    _iface->verbs_common.inl_sge[0].addr      = (uintptr_t)_buf; \
    _iface->verbs_common.inl_sge[0].length    = _len;

#define UCT_RC_VERBS_FILL_AM_BCOPY_WR(_wr, _sge, _length, _wr_opcode) \
    UCT_RC_VERBS_FILL_SGE(_wr, _sge, _length) \
    _wr_opcode = (typeof(_wr_opcode))IBV_WR_SEND;

#define UCT_RC_VERBS_FILL_AM_ZCOPY_WR_IOV(_wr, _sge, _iovlen, _wr_opcode) \
    _wr.sg_list = _sge; \
    _wr.num_sge = _iovlen; \
    _wr_opcode  = (typeof(_wr_opcode))IBV_WR_SEND;

#define UCT_RC_VERBS_FILL_RDMA_WR(_wr, _wr_opcode, _opcode, \
                                  _sge, _length, _raddr, _rkey) \
    UCT_RC_VERBS_FILL_SGE(_wr, _sge, _length) \
    _wr.wr.rdma.remote_addr = _raddr; \
    _wr.wr.rdma.rkey        = uct_ib_md_direct_rkey(_rkey); \
    _wr_opcode              = _opcode; \

#define UCT_RC_VERBS_FILL_RDMA_WR_IOV(_wr, _wr_opcode, _opcode, _sge, _sgelen, \
                                      _raddr, _rkey) \
    _wr.wr.rdma.remote_addr = _raddr; \
    _wr.wr.rdma.rkey        = uct_ib_md_direct_rkey(_rkey); \
    _wr.sg_list             = _sge; \
    _wr.num_sge             = _sgelen; \
    _wr_opcode              = _opcode;

#define UCT_RC_VERBS_FILL_DESC_WR(_wr, _desc) \
    { \
        struct ibv_sge *sge; \
        (_wr)->next    = NULL; \
        sge            = (_wr)->sg_list; \
        sge->addr      = (uintptr_t)(desc + 1); \
        sge->lkey      = (_desc)->lkey; \
    }

#define UCT_RC_VERBS_FILL_ATOMIC_WR(_wr, _wr_opcode, _sge, _opcode, \
                                    _compare_add, _swap, _remote_addr, _rkey) \
    UCT_RC_VERBS_FILL_SGE(_wr, _sge, sizeof(uint64_t)) \
    _wr_opcode                = _opcode; \
    _wr.wr.atomic.compare_add = _compare_add; \
    _wr.wr.atomic.swap        = _swap; \
    _wr.wr.atomic.remote_addr = _remote_addr; \
    _wr.wr.atomic.rkey        = _rkey;  \


#if HAVE_IB_EXT_ATOMICS
static inline void
uct_rc_verbs_fill_ext_atomic_wr(struct ibv_exp_send_wr *wr, struct ibv_sge *sge,
                                int opcode, uint32_t length, uint32_t compare_mask,
                                uint64_t compare_add, uint64_t swap, uint64_t remote_addr,
                                uct_rkey_t rkey, size_t atomic_mr_offset)
{
    sge->length        = length;
    wr->sg_list        = sge;
    wr->num_sge        = 1;
    wr->exp_opcode     = (enum ibv_exp_wr_opcode)opcode;
    wr->comp_mask      = 0;

    wr->ext_op.masked_atomics.log_arg_sz  = ucs_ilog2(length);
    wr->ext_op.masked_atomics.rkey        = uct_ib_resolve_atomic_rkey(rkey,
                                                                       atomic_mr_offset,
                                                                       &remote_addr);
    wr->ext_op.masked_atomics.remote_addr = remote_addr;

    switch (opcode) {
    case IBV_EXP_WR_EXT_MASKED_ATOMIC_CMP_AND_SWP:
        wr->ext_op.masked_atomics.wr_data.inline_data.op.cmp_swap.compare_mask = compare_mask;
        wr->ext_op.masked_atomics.wr_data.inline_data.op.cmp_swap.compare_val  = compare_add;
        wr->ext_op.masked_atomics.wr_data.inline_data.op.cmp_swap.swap_mask    = (uint64_t)(-1);
        wr->ext_op.masked_atomics.wr_data.inline_data.op.cmp_swap.swap_val     = swap;
        break;
    case IBV_EXP_WR_EXT_MASKED_ATOMIC_FETCH_AND_ADD:
        wr->ext_op.masked_atomics.wr_data.inline_data.op.fetch_add.add_val        = compare_add;
        wr->ext_op.masked_atomics.wr_data.inline_data.op.fetch_add.field_boundary = 0;
        break;
    }
}
#endif


#endif

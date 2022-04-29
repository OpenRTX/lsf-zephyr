/*
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <in_common.h>
#include <hal/reg/cmn_sysctrl_reg_venus.h>
#include <hal/venus_ap.h>

#define SYSCTRL_RESET_CP 0xCAFE000B
#define SYSCTRL_RELEASE_CP 0xCAFE100B
#define SYSCTRL_RESET_NPU 0xCAFE000

static struct remoteproc *csk6_rproc_init(struct remoteproc *rproc, struct remoteproc_ops *ops,
					     void *arg)
{
	struct rproc_priv *priv;
	unsigned int cpu_id = *((unsigned int *)arg);

	LPRINTF("%s: node id: %d\n\r", __func__, cpu_id);
	priv = metal_allocate_memory(sizeof(*priv));
	if (!priv)
		return NULL;
	memset(priv, 0, sizeof(*priv));
	priv->rproc = rproc;
	priv->cpu_id = cpu_id;
	priv->rproc->ops = ops;
	metal_list_init(&priv->rproc->mems);
	priv->rproc->priv = priv;
	rproc->state = RPROC_READY;
	return priv->rproc;
}

static void csk6_rproc_remove(struct remoteproc *rproc)
{
	struct rproc_priv *priv;

	priv = (struct rproc_priv *)rproc->priv;
	metal_free_memory(priv);
}

static void *csk6_rproc_mmap(struct remoteproc *rproc, metal_phys_addr_t *pa,
				metal_phys_addr_t *da, size_t size, unsigned int attribute,
				struct metal_io_region **io)
{
	struct remoteproc_mem *mem;
	struct rproc_priv *priv;
	metal_phys_addr_t lpa, lda, lda_end;

	priv = rproc->priv;

	if ((!da || !pa) || (*da == METAL_BAD_PHYS && *pa == METAL_BAD_PHYS))
		return NULL;
	LPRINTF("%s: pa=0x%lx, da=0x%lx, size=0x%x, atrribute=0x%x\n\r", __func__, *pa, *da, size,
		attribute);
	lda = *da;
	lpa = *pa;

	//logic address
	if (lpa == METAL_BAD_PHYS) {
		//for iram address
		if (lda < REMOTE_CORE_BOOT_ADDRESS) {
			lda += REMOTE_VECBASE_OFFSET;
		}
		lpa = lda - PSRAM_LOGIC_ADDRESS_DIFF;
	}

	if (lpa == METAL_BAD_PHYS)
		return NULL;

	mem = metal_allocate_memory(sizeof(*mem));
	if (!mem)
		return NULL;
	mem->pa = lpa;
	mem->da = lda;

	*io = metal_allocate_memory(sizeof(struct metal_io_region));
	if (!*io) {
		metal_free_memory(mem);
		return NULL;
	}

	metal_io_init(*io, (void *)mem->pa, &mem->pa, size, sizeof(metal_phys_addr_t) << 3,
		      attribute, NULL);
	mem->io = *io;
	metal_list_add_tail(&rproc->mems, &mem->node);
	*pa = lpa;
	*da = lda;
	mem->size = size;
	return metal_io_phys_to_virt(*io, mem->pa);
}

int csk6_rproc_start(struct remoteproc *rproc)
{
	SYSCFG_RegDef *cmn_sys_ctrl = (SYSCFG_RegDef *)CMN_SYSCTRL_BASE;

	// set boot address
	cmn_sys_ctrl->REG_CP_CTRL2.all = REMOTE_CORE_BOOT_ADDRESS;
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_STAT_VECTOR_SEL = 0x1;
	// stall HIFI4
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x1;
	// release CP & NPU
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
	if (cmn_sys_ctrl->REG_SW_RESET_NPU.all) { // = 1 NPU reset
		cmn_sys_ctrl->REG_SW_RESET_NPU.all = SYSCTRL_RESET_NPU;
	}

	__DMB();
	__DSB();

	// run cp
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RESET_CP;
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x0;

	//delay for jump cp uart init
	k_msleep(1000);
	return 0;
}

static int csk6_rproc_stop(struct remoteproc *rproc)
{
	/* It is lacking a stop operation in the libPM */
	// stall HIFI4
	SYSCFG_RegDef *cmn_sys_ctrl = (SYSCFG_RegDef *)CMN_SYSCTRL_BASE;
	(void)rproc;

	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x1;

	__DMB();
	__DSB();

	return 0;
}

static int csk6_rproc_shutdown(struct remoteproc *rproc)
{
	struct rproc_priv *priv;
	int ret;
	struct remoteproc_mem *mem;
	struct metal_list *node;
	SYSCFG_RegDef *cmn_sys_ctrl = (SYSCFG_RegDef *)CMN_SYSCTRL_BASE;

	priv = rproc->priv;
	/* Delete all the registered remoteproc memories */
	metal_list_for_each(&rproc->mems, node)
	{
		struct metal_list *tmpnode;
		metal_phys_addr_t pa, pa_end;

		mem = metal_container_of(node, struct remoteproc_mem, node);
		tmpnode = node;
		/* Release TCM resource */
		pa = mem->pa;
		pa_end = metal_io_phys(mem->io, metal_io_region_size(mem->io));
		node = tmpnode->prev;
		metal_list_del(tmpnode);
		metal_free_memory(mem->io);
		metal_free_memory(mem);
	}

	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x1;
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RESET_CP;
	__DMB();
	__DSB();
	return 0;
}

static int csk6_rproc_config(struct remoteproc *rproc, void *data)
{
	return 0;
}

struct remoteproc_ops csk6_rproc_ops = {
	.init = csk6_rproc_init,
	.remove = csk6_rproc_remove,
	.start = csk6_rproc_start,
	.stop = csk6_rproc_stop,
	.shutdown = csk6_rproc_shutdown,
	.mmap = csk6_rproc_mmap,
	.config = csk6_rproc_config,
};

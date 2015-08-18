/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <arpa/inet.h>
#include <getopt.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/virtio_net.h>
#include <linux/virtio_ring.h>
#include <signal.h>
#include <stdint.h>
#include <sys/eventfd.h>
#include <sys/param.h>
#include <unistd.h>

#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <rte_virtio_net.h>

#include "main.h"

#define MAX_QUEUES 512

/* the maximum number of external ports supported */
#define MAX_SUP_PORTS 1

/*
 * Calculate the number of buffers needed per port
 */
#define NUM_MBUFS_PER_PORT ((MAX_QUEUES*RTE_TEST_RX_DESC_DEFAULT) +  		\
							(num_switching_cores*MAX_PKT_BURST) +  			\
							(num_switching_cores*RTE_TEST_TX_DESC_DEFAULT) +\
							(num_switching_cores*MBUF_CACHE_SIZE))

#define MBUF_CACHE_SIZE 128
#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

/*
 * No frame data buffer allocated from host are required for zero copy
 * implementation, guest will allocate the frame data buffer, and vhost
 * directly use it.
 */
#define VIRTIO_DESCRIPTOR_LEN_ZCP 1518
#define MBUF_SIZE_ZCP (VIRTIO_DESCRIPTOR_LEN_ZCP + sizeof(struct rte_mbuf) \
	+ RTE_PKTMBUF_HEADROOM)
#define MBUF_CACHE_SIZE_ZCP 0

#define MAX_PKT_BURST 32 		/* Max burst size for RX/TX */
#define BURST_TX_DRAIN_US 100 	/* TX drain every ~100us */

#define BURST_RX_WAIT_US 15 	/* Defines how long we wait between retries on RX */
#define BURST_RX_RETRIES 4		/* Number of retries on RX. */

#define JUMBO_FRAME_MAX_SIZE    0x2600

/* State of virtio device. */
#define DEVICE_MAC_LEARNING 0
#define DEVICE_RX			1
#define DEVICE_SAFE_REMOVE	2

/* Config_core_flag status definitions. */
#define REQUEST_DEV_REMOVAL 1
#define ACK_DEV_REMOVAL 0

/* Configurable number of RX/TX ring descriptors */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 512

/*
 * Need refine these 2 macros for legacy and DPDK based front end:
 * Max vring avail descriptor/entries from guest - MAX_PKT_BURST
 * And then adjust power 2.
 */
/*
 * For legacy front end, 128 descriptors,
 * half for virtio header, another half for mbuf.
 */
#define RTE_TEST_RX_DESC_DEFAULT_ZCP 32   /* legacy: 32, DPDK virt FE: 128. */
#define RTE_TEST_TX_DESC_DEFAULT_ZCP 64   /* legacy: 64, DPDK virt FE: 64.  */

/* Get first 4 bytes in mbuf headroom. */
#define MBUF_HEADROOM_UINT32(mbuf) (*(uint32_t *)((uint8_t *)(mbuf) \
		+ sizeof(struct rte_mbuf)))

/* true if x is a power of 2 */
#define POWEROF2(x) ((((x)-1) & (x)) == 0)

#define INVALID_PORT_ID 0xFF

/* Max number of devices. Limited by vmdq. */
#define MAX_DEVICES 64

/* Size of buffers used for snprintfs. */
#define MAX_PRINT_BUFF 6072

/* Maximum character device basename size. */
#define MAX_BASENAME_SZ 10

/* Maximum long option length for option parsing. */
#define MAX_LONG_OPT_SZ 64

/* Used to compare MAC addresses. */
#define MAC_ADDR_CMP 0xFFFFFFFFFFFFULL

/* Number of descriptors per cacheline. */
#define DESC_PER_CACHELINE (RTE_CACHE_LINE_SIZE / sizeof(struct vring_desc))

#define MBUF_EXT_MEM(mb)   (RTE_MBUF_FROM_BADDR((mb)->buf_addr) != (mb))

/* mask of enabled ports */
static uint32_t enabled_port_mask = 0;

/* Promiscuous mode */
static uint32_t promiscuous;

/*Number of switching cores enabled*/
static uint32_t num_switching_cores = 0;

/* number of devices/queues to support*/
static uint32_t num_queues = 0;
static uint32_t num_devices;

/*
 * Enable zero copy, pkts buffer will directly dma to hw descriptor,
 * disabled on default.
 */
static uint32_t zero_copy;
static int mergeable;

/* Do vlan strip on host, enabled on default */
static uint32_t vlan_strip = 1;

/* number of descriptors to apply*/
static uint32_t num_rx_descriptor = RTE_TEST_RX_DESC_DEFAULT_ZCP;
static uint32_t num_tx_descriptor = RTE_TEST_TX_DESC_DEFAULT_ZCP;

/* max ring descriptor, ixgbe, i40e, e1000 all are 4096. */
#define MAX_RING_DESC 4096

struct vpool {
	struct rte_mempool *pool;
	struct rte_ring *ring;
	uint32_t buf_size;
} vpool_array[MAX_QUEUES+MAX_QUEUES];

/* Enable VM2VM communications. If this is disabled then the MAC address compare is skipped. */
typedef enum {
	VM2VM_DISABLED = 0,
	VM2VM_SOFTWARE = 1,
	VM2VM_HARDWARE = 2,
	VM2VM_LAST
} vm2vm_type;
static vm2vm_type vm2vm_mode = VM2VM_SOFTWARE;

/* The type of host physical address translated from guest physical address. */
typedef enum {
	PHYS_ADDR_CONTINUOUS = 0,
	PHYS_ADDR_CROSS_SUBREG = 1,
	PHYS_ADDR_INVALID = 2,
	PHYS_ADDR_LAST
} hpa_type;

/* Enable stats. */
static uint32_t enable_stats = 0;
/* Enable retries on RX. */
static uint32_t enable_retry = 1;
/* Specify timeout (in useconds) between retries on RX. */
static uint32_t burst_rx_delay_time = BURST_RX_WAIT_US;
/* Specify the number of retries on RX. */
static uint32_t burst_rx_retry_num = BURST_RX_RETRIES;

/* Character device basename. Can be set by user. */
static char dev_basename[MAX_BASENAME_SZ] = "vhost-net";

/* empty vmdq configuration structure. Filled in programatically */
static struct rte_eth_conf vmdq_conf_default = {
	.rxmode = {
		.mq_mode        = ETH_MQ_RX_VMDQ_ONLY,
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		/*
		 * It is necessary for 1G NIC such as I350,
		 * this fixes bug of ipv4 forwarding in guest can't
		 * forward pakets from one virtio dev to another virtio dev.
		 */
		.hw_vlan_strip  = 1, /**< VLAN strip enabled. */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},

	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
	.rx_adv_conf = {
		/*
		 * should be overridden separately in code with
		 * appropriate values
		 */
		.vmdq_rx_conf = {
			.nb_queue_pools = ETH_8_POOLS,
			.enable_default_pool = 0,
			.default_pool = 0,
			.nb_pool_maps = 0,
			.pool_map = {{0, 0},},
		},
	},
};

static unsigned lcore_ids[RTE_MAX_LCORE];
static uint8_t ports[RTE_MAX_ETHPORTS];
static unsigned num_ports = 0; /**< The number of ports specified in command line */
static uint16_t num_pf_queues, num_vmdq_queues;
static uint16_t vmdq_pool_base, vmdq_queue_base;
static uint16_t queues_per_pool;

static const uint16_t external_pkt_default_vlan_tag = 2000;
const uint16_t vlan_tags[] = {
	1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007,
	1008, 1009, 1010, 1011,	1012, 1013, 1014, 1015,
	1016, 1017, 1018, 1019, 1020, 1021, 1022, 1023,
	1024, 1025, 1026, 1027, 1028, 1029, 1030, 1031,
	1032, 1033, 1034, 1035, 1036, 1037, 1038, 1039,
	1040, 1041, 1042, 1043, 1044, 1045, 1046, 1047,
	1048, 1049, 1050, 1051, 1052, 1053, 1054, 1055,
	1056, 1057, 1058, 1059, 1060, 1061, 1062, 1063,
};

/* ethernet addresses of ports */
static struct ether_addr vmdq_ports_eth_addr[RTE_MAX_ETHPORTS];

/* heads for the main used and free linked lists for the data path. */
static struct virtio_net_data_ll *ll_root_used = NULL;
static struct virtio_net_data_ll *ll_root_free = NULL;

/* Array of data core structures containing information on individual core linked lists. */
static struct lcore_info lcore_info[RTE_MAX_LCORE];

/* Used for queueing bursts of TX packets. */
struct mbuf_table {
	unsigned len;
	unsigned txq_id;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

/* TX queue for each data core. */
struct mbuf_table lcore_tx_queue[RTE_MAX_LCORE];

/* TX queue fori each virtio device for zero copy. */
struct mbuf_table tx_queue_zcp[MAX_QUEUES];

/* Vlan header struct used to insert vlan tags on TX. */
struct vlan_ethhdr {
	unsigned char   h_dest[ETH_ALEN];
	unsigned char   h_source[ETH_ALEN];
	__be16          h_vlan_proto;
	__be16          h_vlan_TCI;
	__be16          h_vlan_encapsulated_proto;
};

/* IPv4 Header */
struct ipv4_hdr {
	uint8_t  version_ihl;		/**< version and header length */
	uint8_t  type_of_service;	/**< type of service */
	uint16_t total_length;		/**< length of packet */
	uint16_t packet_id;		/**< packet ID */
	uint16_t fragment_offset;	/**< fragmentation offset */
	uint8_t  time_to_live;		/**< time to live */
	uint8_t  next_proto_id;		/**< protocol ID */
	uint16_t hdr_checksum;		/**< header checksum */
	uint32_t src_addr;		/**< source address */
	uint32_t dst_addr;		/**< destination address */
} __attribute__((__packed__));

/* Header lengths. */
#define VLAN_HLEN       4
#define VLAN_ETH_HLEN   18

/* Per-device statistics struct */
struct device_statistics {
	uint64_t tx_total;
	rte_atomic64_t rx_total_atomic;
	uint64_t rx_total;
	uint64_t tx;
	rte_atomic64_t rx_atomic;
	uint64_t rx;
} __rte_cache_aligned;
struct device_statistics dev_statistics[MAX_DEVICES];

/*
 * Builds up the correct configuration for VMDQ VLAN pool map
 * according to the pool & queue limits.
 */
static inline int
get_eth_conf(struct rte_eth_conf *eth_conf, uint32_t num_devices)
{
	struct rte_eth_vmdq_rx_conf conf;
	struct rte_eth_vmdq_rx_conf *def_conf =
		&vmdq_conf_default.rx_adv_conf.vmdq_rx_conf;
	unsigned i;

	memset(&conf, 0, sizeof(conf));
	conf.nb_queue_pools = (enum rte_eth_nb_pools)num_devices;
	conf.nb_pool_maps = num_devices;
	conf.enable_loop_back = def_conf->enable_loop_back;
	conf.rx_mode = def_conf->rx_mode;

	for (i = 0; i < conf.nb_pool_maps; i++) {
		conf.pool_map[i].vlan_id = vlan_tags[ i ];
		conf.pool_map[i].pools = (1UL << i);
	}

	(void)(rte_memcpy(eth_conf, &vmdq_conf_default, sizeof(*eth_conf)));
	(void)(rte_memcpy(&eth_conf->rx_adv_conf.vmdq_rx_conf, &conf,
		   sizeof(eth_conf->rx_adv_conf.vmdq_rx_conf)));
	return 0;
}

/*
 * Validate the device number according to the max pool number gotten form
 * dev_info. If the device number is invalid, give the error message and
 * return -1. Each device must have its own pool.
 */
static inline int
validate_num_devices(uint32_t max_nb_devices)
{
	if (num_devices > max_nb_devices) {
		RTE_LOG(ERR, VHOST_PORT, "invalid number of devices\n");
		return -1;
	}
	return 0;
}

/*
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */
static inline int
port_init(uint8_t port)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_conf port_conf;
	struct rte_eth_rxconf *rxconf;
	struct rte_eth_txconf *txconf;
	int16_t rx_rings, tx_rings;
	uint16_t rx_ring_size, tx_ring_size;
	int retval;
	uint16_t q;

	/* The max pool number from dev_info will be used to validate the pool number specified in cmd line */
	rte_eth_dev_info_get (port, &dev_info);

	if (dev_info.max_rx_queues > MAX_QUEUES) {
		rte_exit(EXIT_FAILURE,
			"please define MAX_QUEUES no less than %u in %s\n",
			dev_info.max_rx_queues, __FILE__);
	}

	rxconf = &dev_info.default_rxconf;
	txconf = &dev_info.default_txconf;
	rxconf->rx_drop_en = 1;

	/* Enable vlan offload */
	txconf->txq_flags &= ~ETH_TXQ_FLAGS_NOVLANOFFL;

	/*
	 * Zero copy defers queue RX/TX start to the time when guest
	 * finishes its startup and packet buffers from that guest are
	 * available.
	 */
	if (zero_copy) {
		rxconf->rx_deferred_start = 1;
		rxconf->rx_drop_en = 0;
		txconf->tx_deferred_start = 1;
	}

	/*configure the number of supported virtio devices based on VMDQ limits */
	num_devices = dev_info.max_vmdq_pools;

	if (zero_copy) {
		rx_ring_size = num_rx_descriptor;
		tx_ring_size = num_tx_descriptor;
		tx_rings = dev_info.max_tx_queues;
	} else {
		rx_ring_size = RTE_TEST_RX_DESC_DEFAULT;
		tx_ring_size = RTE_TEST_TX_DESC_DEFAULT;
		tx_rings = (uint16_t)rte_lcore_count();
	}

	retval = validate_num_devices(MAX_DEVICES);
	if (retval < 0)
		return retval;

	/* Get port configuration. */
	retval = get_eth_conf(&port_conf, num_devices);
	if (retval < 0)
		return retval;
	/* NIC queues are divided into pf queues and vmdq queues.  */
	num_pf_queues = dev_info.max_rx_queues - dev_info.vmdq_queue_num;
	queues_per_pool = dev_info.vmdq_queue_num / dev_info.max_vmdq_pools;
	num_vmdq_queues = num_devices * queues_per_pool;
	num_queues = num_pf_queues + num_vmdq_queues;
	vmdq_queue_base = dev_info.vmdq_queue_base;
	vmdq_pool_base  = dev_info.vmdq_pool_base;
	printf("pf queue num: %u, configured vmdq pool num: %u, each vmdq pool has %u queues\n",
		num_pf_queues, num_devices, queues_per_pool);

	if (port >= rte_eth_dev_count()) return -1;

	rx_rings = (uint16_t)dev_info.max_rx_queues;
	/* Configure ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	/* Setup the queues. */
	for (q = 0; q < rx_rings; q ++) {
		retval = rte_eth_rx_queue_setup(port, q, rx_ring_size,
						rte_eth_dev_socket_id(port),
						rxconf,
						vpool_array[q].pool);
		if (retval < 0)
			return retval;
	}
	for (q = 0; q < tx_rings; q ++) {
		retval = rte_eth_tx_queue_setup(port, q, tx_ring_size,
						rte_eth_dev_socket_id(port),
						txconf);
		if (retval < 0)
			return retval;
	}

	/* Start the device. */
	retval  = rte_eth_dev_start(port);
	if (retval < 0) {
		RTE_LOG(ERR, VHOST_DATA, "Failed to start the device.\n");
		return retval;
	}

	if (promiscuous)
		rte_eth_promiscuous_enable(port);

	rte_eth_macaddr_get(port, &vmdq_ports_eth_addr[port]);
	RTE_LOG(INFO, VHOST_PORT, "Max virtio devices supported: %u\n", num_devices);
	RTE_LOG(INFO, VHOST_PORT, "Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
			" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
			(unsigned)port,
			vmdq_ports_eth_addr[port].addr_bytes[0],
			vmdq_ports_eth_addr[port].addr_bytes[1],
			vmdq_ports_eth_addr[port].addr_bytes[2],
			vmdq_ports_eth_addr[port].addr_bytes[3],
			vmdq_ports_eth_addr[port].addr_bytes[4],
			vmdq_ports_eth_addr[port].addr_bytes[5]);

	return 0;
}

/*
 * Set character device basename.
 */
static int
us_vhost_parse_basename(const char *q_arg)
{
	/* parse number string */

	if (strnlen(q_arg, MAX_BASENAME_SZ) > MAX_BASENAME_SZ)
		return -1;
	else
		snprintf((char*)&dev_basename, MAX_BASENAME_SZ, "%s", q_arg);

	return 0;
}

/*
 * Parse the portmask provided at run time.
 */
static int
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	errno = 0;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0') || (errno != 0))
		return -1;

	if (pm == 0)
		return -1;

	return pm;

}

/*
 * Parse num options at run time.
 */
static int
parse_num_opt(const char *q_arg, uint32_t max_valid_value)
{
	char *end = NULL;
	unsigned long num;

	errno = 0;

	/* parse unsigned int string */
	num = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0') || (errno != 0))
		return -1;

	if (num > max_valid_value)
		return -1;

	return num;

}

/*
 * Display usage
 */
static void
us_vhost_usage(const char *prgname)
{
	RTE_LOG(INFO, VHOST_CONFIG, "%s [EAL options] -- -p PORTMASK\n"
	"		--vm2vm [0|1|2]\n"
	"		--rx_retry [0|1] --mergeable [0|1] --stats [0-N]\n"
	"		--dev-basename <name>\n"
	"		--nb-devices ND\n"
	"		-p PORTMASK: Set mask for ports to be used by application\n"
	"		--vm2vm [0|1|2]: disable/software(default)/hardware vm2vm comms\n"
	"		--rx-retry [0|1]: disable/enable(default) retries on rx. Enable retry if destintation queue is full\n"
	"		--rx-retry-delay [0-N]: timeout(in usecond) between retries on RX. This makes effect only if retries on rx enabled\n"
	"		--rx-retry-num [0-N]: the number of retries on rx. This makes effect only if retries on rx enabled\n"
	"		--mergeable [0|1]: disable(default)/enable RX mergeable buffers\n"
	"		--vlan-strip [0|1]: disable/enable(default) RX VLAN strip on host\n"
	"		--stats [0-N]: 0: Disable stats, N: Time in seconds to print stats\n"
	"		--dev-basename: The basename to be used for the character device.\n"
	"		--zero-copy [0|1]: disable(default)/enable rx/tx "
			"zero copy\n"
	"		--rx-desc-num [0-N]: the number of descriptors on rx, "
			"used only when zero copy is enabled.\n"
	"		--tx-desc-num [0-N]: the number of descriptors on tx, "
			"used only when zero copy is enabled.\n",
	       prgname);
}

/*
 * Parse the arguments given in the command line of the application.
 */
static int
us_vhost_parse_args(int argc, char **argv)
{
	int opt, ret;
	int option_index;
	unsigned i;
	const char *prgname = argv[0];
	static struct option long_option[] = {
		{"vm2vm", required_argument, NULL, 0},
		{"rx-retry", required_argument, NULL, 0},
		{"rx-retry-delay", required_argument, NULL, 0},
		{"rx-retry-num", required_argument, NULL, 0},
		{"mergeable", required_argument, NULL, 0},
		{"vlan-strip", required_argument, NULL, 0},
		{"stats", required_argument, NULL, 0},
		{"dev-basename", required_argument, NULL, 0},
		{"zero-copy", required_argument, NULL, 0},
		{"rx-desc-num", required_argument, NULL, 0},
		{"tx-desc-num", required_argument, NULL, 0},
		{NULL, 0, 0, 0},
	};

	/* Parse command line */
	while ((opt = getopt_long(argc, argv, "p:P",
			long_option, &option_index)) != EOF) {
		switch (opt) {
		/* Portmask */
		case 'p':
			enabled_port_mask = parse_portmask(optarg);
			if (enabled_port_mask == 0) {
				RTE_LOG(INFO, VHOST_CONFIG, "Invalid portmask\n");
				us_vhost_usage(prgname);
				return -1;
			}
			break;

		case 'P':
			promiscuous = 1;
			vmdq_conf_default.rx_adv_conf.vmdq_rx_conf.rx_mode =
				ETH_VMDQ_ACCEPT_BROADCAST |
				ETH_VMDQ_ACCEPT_MULTICAST;
			rte_vhost_feature_enable(1ULL << VIRTIO_NET_F_CTRL_RX);

			break;

		case 0:
			/* Enable/disable vm2vm comms. */
			if (!strncmp(long_option[option_index].name, "vm2vm",
				MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, (VM2VM_LAST - 1));
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for "
						"vm2vm [0|1|2]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					vm2vm_mode = (vm2vm_type)ret;
				}
			}

			/* Enable/disable retries on RX. */
			if (!strncmp(long_option[option_index].name, "rx-retry", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for rx-retry [0|1]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					enable_retry = ret;
				}
			}

			/* Specify the retries delay time (in useconds) on RX. */
			if (!strncmp(long_option[option_index].name, "rx-retry-delay", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, INT32_MAX);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for rx-retry-delay [0-N]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					burst_rx_delay_time = ret;
				}
			}

			/* Specify the retries number on RX. */
			if (!strncmp(long_option[option_index].name, "rx-retry-num", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, INT32_MAX);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for rx-retry-num [0-N]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					burst_rx_retry_num = ret;
				}
			}

			/* Enable/disable RX mergeable buffers. */
			if (!strncmp(long_option[option_index].name, "mergeable", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for mergeable [0|1]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					mergeable = !!ret;
					if (ret) {
						vmdq_conf_default.rxmode.jumbo_frame = 1;
						vmdq_conf_default.rxmode.max_rx_pkt_len
							= JUMBO_FRAME_MAX_SIZE;
					}
				}
			}

			/* Enable/disable RX VLAN strip on host. */
			if (!strncmp(long_option[option_index].name,
				"vlan-strip", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument for VLAN strip [0|1]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					vlan_strip = !!ret;
					vmdq_conf_default.rxmode.hw_vlan_strip =
						vlan_strip;
				}
			}

			/* Enable/disable stats. */
			if (!strncmp(long_option[option_index].name, "stats", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, INT32_MAX);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for stats [0..N]\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					enable_stats = ret;
				}
			}

			/* Set character device basename. */
			if (!strncmp(long_option[option_index].name, "dev-basename", MAX_LONG_OPT_SZ)) {
				if (us_vhost_parse_basename(optarg) == -1) {
					RTE_LOG(INFO, VHOST_CONFIG, "Invalid argument for character device basename (Max %d characters)\n", MAX_BASENAME_SZ);
					us_vhost_usage(prgname);
					return -1;
				}
			}

			/* Enable/disable rx/tx zero copy. */
			if (!strncmp(long_option[option_index].name,
				"zero-copy", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, 1);
				if (ret == -1) {
					RTE_LOG(INFO, VHOST_CONFIG,
						"Invalid argument"
						" for zero-copy [0|1]\n");
					us_vhost_usage(prgname);
					return -1;
				} else
					zero_copy = ret;
			}

			/* Specify the descriptor number on RX. */
			if (!strncmp(long_option[option_index].name,
				"rx-desc-num", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, MAX_RING_DESC);
				if ((ret == -1) || (!POWEROF2(ret))) {
					RTE_LOG(INFO, VHOST_CONFIG,
					"Invalid argument for rx-desc-num[0-N],"
					"power of 2 required.\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					num_rx_descriptor = ret;
				}
			}

			/* Specify the descriptor number on TX. */
			if (!strncmp(long_option[option_index].name,
				"tx-desc-num", MAX_LONG_OPT_SZ)) {
				ret = parse_num_opt(optarg, MAX_RING_DESC);
				if ((ret == -1) || (!POWEROF2(ret))) {
					RTE_LOG(INFO, VHOST_CONFIG,
					"Invalid argument for tx-desc-num [0-N],"
					"power of 2 required.\n");
					us_vhost_usage(prgname);
					return -1;
				} else {
					num_tx_descriptor = ret;
				}
			}

			break;

			/* Invalid option - print options. */
		default:
			us_vhost_usage(prgname);
			return -1;
		}
	}

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (enabled_port_mask & (1 << i))
			ports[num_ports++] = (uint8_t)i;
	}

	if ((num_ports ==  0) || (num_ports > MAX_SUP_PORTS)) {
		RTE_LOG(INFO, VHOST_PORT, "Current enabled port number is %u,"
			"but only %u port can be enabled\n",num_ports, MAX_SUP_PORTS);
		return -1;
	}

	if ((zero_copy == 1) && (vm2vm_mode == VM2VM_SOFTWARE)) {
		RTE_LOG(INFO, VHOST_PORT,
			"Vhost zero copy doesn't support software vm2vm,"
			"please specify 'vm2vm 2' to use hardware vm2vm.\n");
		return -1;
	}

	if ((zero_copy == 1) && (vmdq_conf_default.rxmode.jumbo_frame == 1)) {
		RTE_LOG(INFO, VHOST_PORT,
			"Vhost zero copy doesn't support jumbo frame,"
			"please specify '--mergeable 0' to disable the "
			"mergeable feature.\n");
		return -1;
	}

	return 0;
}

/*
 * Update the global var NUM_PORTS and array PORTS according to system ports number
 * and return valid ports number
 */
static unsigned check_ports_num(unsigned nb_ports)
{
	unsigned valid_num_ports = num_ports;
	unsigned portid;

	if (num_ports > nb_ports) {
		RTE_LOG(INFO, VHOST_PORT, "\nSpecified port number(%u) exceeds total system port number(%u)\n",
			num_ports, nb_ports);
		num_ports = nb_ports;
	}

	for (portid = 0; portid < num_ports; portid ++) {
		if (ports[portid] >= nb_ports) {
			RTE_LOG(INFO, VHOST_PORT, "\nSpecified port ID(%u) exceeds max system port ID(%u)\n",
				ports[portid], (nb_ports - 1));
			ports[portid] = INVALID_PORT_ID;
			valid_num_ports--;
		}
	}
	return valid_num_ports;
}

/*
 * Macro to print out packet contents. Wrapped in debug define so that the
 * data path is not effected when debug is disabled.
 */
#ifdef DEBUG
#define PRINT_PACKET(device, addr, size, header) do {																\
	char *pkt_addr = (char*)(addr);																					\
	unsigned int index;																								\
	char packet[MAX_PRINT_BUFF];																					\
																													\
	if ((header))																									\
		snprintf(packet, MAX_PRINT_BUFF, "(%"PRIu64") Header size %d: ", (device->device_fh), (size));				\
	else																											\
		snprintf(packet, MAX_PRINT_BUFF, "(%"PRIu64") Packet size %d: ", (device->device_fh), (size));				\
	for (index = 0; index < (size); index++) {																		\
		snprintf(packet + strnlen(packet, MAX_PRINT_BUFF), MAX_PRINT_BUFF - strnlen(packet, MAX_PRINT_BUFF),	\
			"%02hhx ", pkt_addr[index]);																			\
	}																												\
	snprintf(packet + strnlen(packet, MAX_PRINT_BUFF), MAX_PRINT_BUFF - strnlen(packet, MAX_PRINT_BUFF), "\n");	\
																													\
	LOG_DEBUG(VHOST_DATA, "%s", packet);																					\
} while(0)
#else
#define PRINT_PACKET(device, addr, size, header) do{} while(0)
#endif

/*
 * Function to convert guest physical addresses to vhost physical addresses.
 * This is used to convert virtio buffer addresses.
 */
static inline uint64_t __attribute__((always_inline))
gpa_to_hpa(struct vhost_dev  *vdev, uint64_t guest_pa,
	uint32_t buf_len, hpa_type *addr_type)
{
	struct virtio_memory_regions_hpa *region;
	uint32_t regionidx;
	uint64_t vhost_pa = 0;

	*addr_type = PHYS_ADDR_INVALID;

	for (regionidx = 0; regionidx < vdev->nregions_hpa; regionidx++) {
		region = &vdev->regions_hpa[regionidx];
		if ((guest_pa >= region->guest_phys_address) &&
			(guest_pa <= region->guest_phys_address_end)) {
			vhost_pa = region->host_phys_addr_offset + guest_pa;
			if (likely((guest_pa + buf_len - 1)
				<= region->guest_phys_address_end))
				*addr_type = PHYS_ADDR_CONTINUOUS;
			else
				*addr_type = PHYS_ADDR_CROSS_SUBREG;
			break;
		}
	}

	LOG_DEBUG(VHOST_DATA, "(%"PRIu64") GPA %p| HPA %p\n",
		vdev->dev->device_fh, (void *)(uintptr_t)guest_pa,
		(void *)(uintptr_t)vhost_pa);

	return vhost_pa;
}

/*
 * Compares a packet destination MAC address to a device MAC address.
 */
static inline int __attribute__((always_inline))
ether_addr_cmp(struct ether_addr *ea, struct ether_addr *eb)
{
	return (((*(uint64_t *)ea ^ *(uint64_t *)eb) & MAC_ADDR_CMP) == 0);
}


/*
 * Removes MAC address and vlan tag from VMDQ. Ensures that nothing is adding buffers to the RX
 * queue before disabling RX on the device.
 */
static inline void
unlink_vmdq(struct vhost_dev *vdev)
{
	unsigned i = 0;
	unsigned rx_count;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

	if (vdev->ready == DEVICE_RX) {
		/*clear MAC and VLAN settings*/
		rte_eth_dev_mac_addr_remove(ports[0], &vdev->mac_address);
		for (i = 0; i < 6; i++)
			vdev->mac_address.addr_bytes[i] = 0;

		vdev->vlan_tag = 0;

		/*Clear out the receive buffers*/
		rx_count = rte_eth_rx_burst(ports[0],
					(uint16_t)vdev->vmdq_rx_q, pkts_burst, MAX_PKT_BURST);

		while (rx_count) {
			for (i = 0; i < rx_count; i++)
				rte_pktmbuf_free(pkts_burst[i]);

			rx_count = rte_eth_rx_burst(ports[0],
					(uint16_t)vdev->vmdq_rx_q, pkts_burst, MAX_PKT_BURST);
		}

		vdev->ready = DEVICE_MAC_LEARNING;
	}
}

/*
 * Check if the packet destination MAC address is for a local device. If so then put
 * the packet on that devices RX queue. If not then return.
 */

/*
 * Check if the destination MAC of a packet is one local VM,
 * and get its vlan tag, and offset if it is.
 */
static inline int __attribute__((always_inline))
find_local_dest(struct virtio_net *dev, struct rte_mbuf *m,
	uint32_t *offset, uint16_t *vlan_tag)
{
	struct virtio_net_data_ll *dev_ll = ll_root_used;
	struct ether_hdr *pkt_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	while (dev_ll != NULL) {
		if ((dev_ll->vdev->ready == DEVICE_RX)
			&& ether_addr_cmp(&(pkt_hdr->d_addr),
		&dev_ll->vdev->mac_address)) {
			/*
			 * Drop the packet if the TX packet is
			 * destined for the TX device.
			 */
			if (dev_ll->vdev->dev->device_fh == dev->device_fh) {
				LOG_DEBUG(VHOST_DATA,
				"(%"PRIu64") TX: Source and destination"
				" MAC addresses are the same. Dropping "
				"packet.\n",
				dev_ll->vdev->dev->device_fh);
				return -1;
			}

			/*
			 * HW vlan strip will reduce the packet length
			 * by minus length of vlan tag, so need restore
			 * the packet length by plus it.
			 */
			*offset = VLAN_HLEN;
			*vlan_tag =
			(uint16_t)
			vlan_tags[(uint16_t)dev_ll->vdev->dev->device_fh];

			LOG_DEBUG(VHOST_DATA,
			"(%"PRIu64") TX: pkt to local VM device id:"
			"(%"PRIu64") vlan tag: %d.\n",
			dev->device_fh, dev_ll->vdev->dev->device_fh,
			vlan_tag);

			break;
		}
		dev_ll = dev_ll->next;
	}
	return 0;
}

/*
 * This function routes the TX packet to the correct interface. This may be a local device
 * or the physical port.
 */

/*
 * This function is called by each data core. It handles all RX/TX registered with the
 * core. For TX the specific lcore linked list is used. For RX, MAC addresses are compared
 * with all devices in the main linked list.
 */
static int
switch_worker(__attribute__((unused)) void *arg)
{
	struct rte_mempool *mbuf_pool = arg;
	struct virtio_net *dev = NULL;
	struct vhost_dev *rvdev,*vdev = NULL;
	struct vhost_dev *vdev_list[2];
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct virtio_net_data_ll *dev_ll;
	struct mbuf_table *tx_q;
	volatile struct lcore_ll_info *lcore_ll;
	//const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;
	uint64_t prev_tsc;
	unsigned  i;
	const uint16_t lcore_id = rte_lcore_id();
	const uint16_t num_cores = (uint16_t)rte_lcore_count();

	uint16_t tx_count;


	RTE_LOG(INFO, VHOST_DATA, "Procesing on Core %u started\n", lcore_id);
	lcore_ll = lcore_info[lcore_id].lcore_ll;
	prev_tsc = 0;

	tx_q = &lcore_tx_queue[lcore_id];
	for (i = 0; i < num_cores; i ++) {
		if (lcore_ids[i] == lcore_id) {
			tx_q->txq_id = i;
			break;
		}
	}
	vdev_list[0]=vdev_list[1]=0;
	while(1) {

		rte_prefetch0(lcore_ll->ll_root_used);
		/*
		 * Inform the configuration core that we have exited the linked list and that no devices are
		 * in use if requested.
		 */
		if (lcore_ll->dev_removal_flag == REQUEST_DEV_REMOVAL)
			lcore_ll->dev_removal_flag = ACK_DEV_REMOVAL;


		dev_ll = lcore_ll->ll_root_used;
		i=0;
		while (dev_ll != NULL) {
			if (i>1){
				break;
			}
			/*get virtio device ID*/
			vdev = dev_ll->vdev;
			vdev_list[i]=vdev;
			i++;
			dev_ll = dev_ll->next;
		}

		for (i=0; i<2; i++) {
			if (vdev_list[i]==0) break;
			/*get virtio device ID*/
			vdev = vdev_list[i];
			if (i==0){
				rvdev = vdev_list[1];
			}else{
				rvdev = vdev_list[0];
			}
			dev = vdev->dev;

			if (likely(!vdev->remove)) {
				/* Handle guest TX*/
				tx_count = rte_vhost_dequeue_burst(dev, VIRTIO_TXQ, mbuf_pool,
						pkts_burst, MAX_PKT_BURST);
				if (rvdev != 0 && tx_count) {
					//printf(" Transfering to :%d   pkts:%d\n", i, tx_count);
					rte_vhost_enqueue_burst(rvdev->dev, VIRTIO_RXQ, pkts_burst,
							tx_count);
				}
				while (tx_count) {
					//printf(" Freeing packet  to :%d   pkts:%d\n", i, tx_count);
					rte_pktmbuf_free(pkts_burst[--tx_count]);
				}

			}
		}
	}

	return 0;
}

/*
 * This function gets available ring number for zero copy rx.
 * Only one thread will call this funciton for a paticular virtio device,
 * so, it is designed as non-thread-safe function.
 */
static inline uint32_t __attribute__((always_inline))
get_available_ring_num_zcp(struct virtio_net *dev)
{
	struct vhost_virtqueue *vq = dev->virtqueue[VIRTIO_RXQ];
	uint16_t avail_idx;

	avail_idx = *((volatile uint16_t *)&vq->avail->idx);
	return (uint32_t)(avail_idx - vq->last_used_idx_res);
}

/*
 * This function gets available ring index for zero copy rx,
 * it will retry 'burst_rx_retry_num' times till it get enough ring index.
 * Only one thread will call this funciton for a paticular virtio device,
 * so, it is designed as non-thread-safe function.
 */
static inline uint32_t __attribute__((always_inline))
get_available_ring_index_zcp(struct virtio_net *dev,
	uint16_t *res_base_idx, uint32_t count)
{
	struct vhost_virtqueue *vq = dev->virtqueue[VIRTIO_RXQ];
	uint16_t avail_idx;
	uint32_t retry = 0;
	uint16_t free_entries;

	*res_base_idx = vq->last_used_idx_res;
	avail_idx = *((volatile uint16_t *)&vq->avail->idx);
	free_entries = (avail_idx - *res_base_idx);

	LOG_DEBUG(VHOST_DATA, "(%"PRIu64") in get_available_ring_index_zcp: "
			"avail idx: %d, "
			"res base idx:%d, free entries:%d\n",
			dev->device_fh, avail_idx, *res_base_idx,
			free_entries);

	/*
	 * If retry is enabled and the queue is full then we wait
	 * and retry to avoid packet loss.
	 */
	if (enable_retry && unlikely(count > free_entries)) {
		for (retry = 0; retry < burst_rx_retry_num; retry++) {
			rte_delay_us(burst_rx_delay_time);
			avail_idx = *((volatile uint16_t *)&vq->avail->idx);
			free_entries = (avail_idx - *res_base_idx);
			if (count <= free_entries)
				break;
		}
	}

	/*check that we have enough buffers*/
	if (unlikely(count > free_entries))
		count = free_entries;

	if (unlikely(count == 0)) {
		LOG_DEBUG(VHOST_DATA,
			"(%"PRIu64") Fail in get_available_ring_index_zcp: "
			"avail idx: %d, res base idx:%d, free entries:%d\n",
			dev->device_fh, avail_idx,
			*res_base_idx, free_entries);
		return 0;
	}

	vq->last_used_idx_res = *res_base_idx + count;

	return count;
}

/*
 * This function put descriptor back to used list.
 */
static inline void __attribute__((always_inline))
put_desc_to_used_list_zcp(struct vhost_virtqueue *vq, uint16_t desc_idx)
{
	uint16_t res_cur_idx = vq->last_used_idx;
	vq->used->ring[res_cur_idx & (vq->size - 1)].id = (uint32_t)desc_idx;
	vq->used->ring[res_cur_idx & (vq->size - 1)].len = 0;
	rte_compiler_barrier();
	*(volatile uint16_t *)&vq->used->idx += 1;
	vq->last_used_idx += 1;

	/* Kick the guest if necessary. */
	if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT))
		eventfd_write((int)vq->callfd, 1);
}

/*
 * This function get available descriptor from vitio vring and un-attached mbuf
 * from vpool->ring, and then attach them together. It needs adjust the offset
 * for buff_addr and phys_addr accroding to PMD implementation, otherwise the
 * frame data may be put to wrong location in mbuf.
 */
static inline void __attribute__((always_inline))
attach_rxmbuf_zcp(struct virtio_net *dev)
{
	uint16_t res_base_idx, desc_idx;
	uint64_t buff_addr, phys_addr;
	struct vhost_virtqueue *vq;
	struct vring_desc *desc;
	struct rte_mbuf *mbuf = NULL;
	struct vpool *vpool;
	hpa_type addr_type;
	struct vhost_dev *vdev = (struct vhost_dev *)dev->priv;

	vpool = &vpool_array[vdev->vmdq_rx_q];
	vq = dev->virtqueue[VIRTIO_RXQ];

	do {
		if (unlikely(get_available_ring_index_zcp(vdev->dev, &res_base_idx,
				1) != 1))
			return;
		desc_idx = vq->avail->ring[(res_base_idx) & (vq->size - 1)];

		desc = &vq->desc[desc_idx];
		if (desc->flags & VRING_DESC_F_NEXT) {
			desc = &vq->desc[desc->next];
			buff_addr = gpa_to_vva(dev, desc->addr);
			phys_addr = gpa_to_hpa(vdev, desc->addr, desc->len,
					&addr_type);
		} else {
			buff_addr = gpa_to_vva(dev,
					desc->addr + vq->vhost_hlen);
			phys_addr = gpa_to_hpa(vdev,
					desc->addr + vq->vhost_hlen,
					desc->len, &addr_type);
		}

		if (unlikely(addr_type == PHYS_ADDR_INVALID)) {
			RTE_LOG(ERR, VHOST_DATA, "(%"PRIu64") Invalid frame buffer"
				" address found when attaching RX frame buffer"
				" address!\n", dev->device_fh);
			put_desc_to_used_list_zcp(vq, desc_idx);
			continue;
		}

		/*
		 * Check if the frame buffer address from guest crosses
		 * sub-region or not.
		 */
		if (unlikely(addr_type == PHYS_ADDR_CROSS_SUBREG)) {
			RTE_LOG(ERR, VHOST_DATA,
				"(%"PRIu64") Frame buffer address cross "
				"sub-regioin found when attaching RX frame "
				"buffer address!\n",
				dev->device_fh);
			put_desc_to_used_list_zcp(vq, desc_idx);
			continue;
		}
	} while (unlikely(phys_addr == 0));

	rte_ring_sc_dequeue(vpool->ring, (void **)&mbuf);
	if (unlikely(mbuf == NULL)) {
		LOG_DEBUG(VHOST_DATA,
			"(%"PRIu64") in attach_rxmbuf_zcp: "
			"ring_sc_dequeue fail.\n",
			dev->device_fh);
		put_desc_to_used_list_zcp(vq, desc_idx);
		return;
	}

	if (unlikely(vpool->buf_size > desc->len)) {
		LOG_DEBUG(VHOST_DATA,
			"(%"PRIu64") in attach_rxmbuf_zcp: frame buffer "
			"length(%d) of descriptor idx: %d less than room "
			"size required: %d\n",
			dev->device_fh, desc->len, desc_idx, vpool->buf_size);
		put_desc_to_used_list_zcp(vq, desc_idx);
		rte_ring_sp_enqueue(vpool->ring, (void *)mbuf);
		return;
	}

	mbuf->buf_addr = (void *)(uintptr_t)(buff_addr - RTE_PKTMBUF_HEADROOM);
	mbuf->data_off = RTE_PKTMBUF_HEADROOM;
	mbuf->buf_physaddr = phys_addr - RTE_PKTMBUF_HEADROOM;
	mbuf->data_len = desc->len;
	MBUF_HEADROOM_UINT32(mbuf) = (uint32_t)desc_idx;

	LOG_DEBUG(VHOST_DATA,
		"(%"PRIu64") in attach_rxmbuf_zcp: res base idx:%d, "
		"descriptor idx:%d\n",
		dev->device_fh, res_base_idx, desc_idx);

	__rte_mbuf_raw_free(mbuf);

	return;
}

/*
 * Detach an attched packet mbuf -
 *  - restore original mbuf address and length values.
 *  - reset pktmbuf data and data_len to their default values.
 *  All other fields of the given packet mbuf will be left intact.
 *
 * @param m
 *   The attached packet mbuf.
 */
static inline void pktmbuf_detach_zcp(struct rte_mbuf *m)
{
	const struct rte_mempool *mp = m->pool;
	void *buf = RTE_MBUF_TO_BADDR(m);
	uint32_t buf_ofs;
	uint32_t buf_len = mp->elt_size - sizeof(*m);
	m->buf_physaddr = rte_mempool_virt2phy(mp, m) + sizeof(*m);

	m->buf_addr = buf;
	m->buf_len = (uint16_t)buf_len;

	buf_ofs = (RTE_PKTMBUF_HEADROOM <= m->buf_len) ?
			RTE_PKTMBUF_HEADROOM : m->buf_len;
	m->data_off = buf_ofs;

	m->data_len = 0;
}

/*
 * This function is called after packets have been transimited. It fetchs mbuf
 * from vpool->pool, detached it and put into vpool->ring. It also update the
 * used index and kick the guest if necessary.
 */
static inline uint32_t __attribute__((always_inline))
txmbuf_clean_zcp(struct virtio_net *dev, struct vpool *vpool)
{
	struct rte_mbuf *mbuf;
	struct vhost_virtqueue *vq = dev->virtqueue[VIRTIO_TXQ];
	uint32_t used_idx = vq->last_used_idx & (vq->size - 1);
	uint32_t index = 0;
	uint32_t mbuf_count = rte_mempool_count(vpool->pool);

	LOG_DEBUG(VHOST_DATA,
		"(%"PRIu64") in txmbuf_clean_zcp: mbuf count in mempool before "
		"clean is: %d\n",
		dev->device_fh, mbuf_count);
	LOG_DEBUG(VHOST_DATA,
		"(%"PRIu64") in txmbuf_clean_zcp: mbuf count in  ring before "
		"clean  is : %d\n",
		dev->device_fh, rte_ring_count(vpool->ring));

	for (index = 0; index < mbuf_count; index++) {
		mbuf = __rte_mbuf_raw_alloc(vpool->pool);
		if (likely(MBUF_EXT_MEM(mbuf)))
			pktmbuf_detach_zcp(mbuf);
		rte_ring_sp_enqueue(vpool->ring, mbuf);

		/* Update used index buffer information. */
		vq->used->ring[used_idx].id = MBUF_HEADROOM_UINT32(mbuf);
		vq->used->ring[used_idx].len = 0;

		used_idx = (used_idx + 1) & (vq->size - 1);
	}

	LOG_DEBUG(VHOST_DATA,
		"(%"PRIu64") in txmbuf_clean_zcp: mbuf count in mempool after "
		"clean is: %d\n",
		dev->device_fh, rte_mempool_count(vpool->pool));
	LOG_DEBUG(VHOST_DATA,
		"(%"PRIu64") in txmbuf_clean_zcp: mbuf count in  ring after "
		"clean  is : %d\n",
		dev->device_fh, rte_ring_count(vpool->ring));
	LOG_DEBUG(VHOST_DATA,
		"(%"PRIu64") in txmbuf_clean_zcp: before updated "
		"vq->last_used_idx:%d\n",
		dev->device_fh, vq->last_used_idx);

	vq->last_used_idx += mbuf_count;

	LOG_DEBUG(VHOST_DATA,
		"(%"PRIu64") in txmbuf_clean_zcp: after updated "
		"vq->last_used_idx:%d\n",
		dev->device_fh, vq->last_used_idx);

	rte_compiler_barrier();

	*(volatile uint16_t *)&vq->used->idx += mbuf_count;

	/* Kick guest if required. */
	if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT))
		eventfd_write((int)vq->callfd, 1);

	return 0;
}

/*
 * This function is called when a virtio device is destroy.
 * It fetchs mbuf from vpool->pool, and detached it, and put into vpool->ring.
 */
static void mbuf_destroy_zcp(struct vpool *vpool)
{
	struct rte_mbuf *mbuf = NULL;
	uint32_t index, mbuf_count = rte_mempool_count(vpool->pool);

	LOG_DEBUG(VHOST_CONFIG,
		"in mbuf_destroy_zcp: mbuf count in mempool before "
		"mbuf_destroy_zcp is: %d\n",
		mbuf_count);
	LOG_DEBUG(VHOST_CONFIG,
		"in mbuf_destroy_zcp: mbuf count in  ring before "
		"mbuf_destroy_zcp  is : %d\n",
		rte_ring_count(vpool->ring));

	for (index = 0; index < mbuf_count; index++) {
		mbuf = __rte_mbuf_raw_alloc(vpool->pool);
		if (likely(mbuf != NULL)) {
			if (likely(MBUF_EXT_MEM(mbuf)))
				pktmbuf_detach_zcp(mbuf);
			rte_ring_sp_enqueue(vpool->ring, (void *)mbuf);
		}
	}

	LOG_DEBUG(VHOST_CONFIG,
		"in mbuf_destroy_zcp: mbuf count in mempool after "
		"mbuf_destroy_zcp is: %d\n",
		rte_mempool_count(vpool->pool));
	LOG_DEBUG(VHOST_CONFIG,
		"in mbuf_destroy_zcp: mbuf count in ring after "
		"mbuf_destroy_zcp is : %d\n",
		rte_ring_count(vpool->ring));
}

/*
 * This function update the use flag and counter.
 */

/*
 * This function routes the TX packet to the correct interface.
 * This may be a local device or the physical port.
 */



/*
 * Add an entry to a used linked list. A free entry must first be found
 * in the free linked list using get_data_ll_free_entry();
 */
static void
add_data_ll_entry(struct virtio_net_data_ll **ll_root_addr,
	struct virtio_net_data_ll *ll_dev)
{
	struct virtio_net_data_ll *ll = *ll_root_addr;

	/* Set next as NULL and use a compiler barrier to avoid reordering. */
	ll_dev->next = NULL;
	rte_compiler_barrier();

	/* If ll == NULL then this is the first device. */
	if (ll) {
		/* Increment to the tail of the linked list. */
		while ((ll->next != NULL) )
			ll = ll->next;

		ll->next = ll_dev;
	} else {
		*ll_root_addr = ll_dev;
	}
}

/*
 * Remove an entry from a used linked list. The entry must then be added to
 * the free linked list using put_data_ll_free_entry().
 */
static void
rm_data_ll_entry(struct virtio_net_data_ll **ll_root_addr,
	struct virtio_net_data_ll *ll_dev,
	struct virtio_net_data_ll *ll_dev_last)
{
	struct virtio_net_data_ll *ll = *ll_root_addr;

	if (unlikely((ll == NULL) || (ll_dev == NULL)))
		return;

	if (ll_dev == ll)
		*ll_root_addr = ll_dev->next;
	else
		if (likely(ll_dev_last != NULL))
			ll_dev_last->next = ll_dev->next;
		else
			RTE_LOG(ERR, VHOST_CONFIG, "Remove entry form ll failed.\n");
}

/*
 * Find and return an entry from the free linked list.
 */
static struct virtio_net_data_ll *
get_data_ll_free_entry(struct virtio_net_data_ll **ll_root_addr)
{
	if (ll_root_addr ==0 ) return 0; /* jana */
	struct virtio_net_data_ll *ll_free = *ll_root_addr;
	struct virtio_net_data_ll *ll_dev;

	if (ll_free == NULL)
		return NULL;

	ll_dev = ll_free;
	*ll_root_addr = ll_free->next;

	return ll_dev;
}

/*
 * Place an entry back on to the free linked list.
 */
static void
put_data_ll_free_entry(struct virtio_net_data_ll **ll_root_addr,
	struct virtio_net_data_ll *ll_dev)
{
	struct virtio_net_data_ll *ll_free = *ll_root_addr;

	if (ll_dev == NULL)
		return;

	ll_dev->next = ll_free;
	*ll_root_addr = ll_dev;
}

/*
 * Creates a linked list of a given size.
 */
static struct virtio_net_data_ll *
alloc_data_ll(uint32_t size)
{
	struct virtio_net_data_ll *ll_new;
	uint32_t i;

	if (size==0) size=20; /* jana */
	/* Malloc and then chain the linked list. */
	ll_new = malloc(size * sizeof(struct virtio_net_data_ll));
	if (ll_new == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG, "Failed to allocate memory for ll_new.\n");
		return NULL;
	}

	for (i = 0; i < size - 1; i++) {
		ll_new[i].vdev = NULL;
		ll_new[i].next = &ll_new[i+1];
	}
	ll_new[i].next = NULL;

	return (ll_new);
}

/*
 * Create the main linked list along with each individual cores linked list. A used and a free list
 * are created to manage entries.
 */
static int
init_data_ll (void)
{
	int lcore;

	RTE_LCORE_FOREACH_SLAVE(lcore) {
		lcore_info[lcore].lcore_ll = malloc(sizeof(struct lcore_ll_info));
		if (lcore_info[lcore].lcore_ll == NULL) {
			RTE_LOG(ERR, VHOST_CONFIG, "Failed to allocate memory for lcore_ll.\n");
			return -1;
		}

		lcore_info[lcore].lcore_ll->device_num = 0;
		lcore_info[lcore].lcore_ll->dev_removal_flag = ACK_DEV_REMOVAL;
		lcore_info[lcore].lcore_ll->ll_root_used = NULL;
		if (num_devices % num_switching_cores)
			lcore_info[lcore].lcore_ll->ll_root_free = alloc_data_ll((num_devices / num_switching_cores) + 1);
		else
			lcore_info[lcore].lcore_ll->ll_root_free = alloc_data_ll(num_devices / num_switching_cores);
	}

	/* Allocate devices up to a maximum of MAX_DEVICES. */
	ll_root_free = alloc_data_ll(MIN((num_devices), MAX_DEVICES));

	return 0;
}

/*
 * Remove a device from the specific data core linked list and from the main linked list. Synchonization
 * occurs through the use of the lcore dev_removal_flag. Device is made volatile here to avoid re-ordering
 * of dev->remove=1 which can cause an infinite loop in the rte_pause loop.
 */
static void
destroy_device (volatile struct virtio_net *dev)
{
	struct virtio_net_data_ll *ll_lcore_dev_cur;
	struct virtio_net_data_ll *ll_main_dev_cur;
	struct virtio_net_data_ll *ll_lcore_dev_last = NULL;
	struct virtio_net_data_ll *ll_main_dev_last = NULL;
	struct vhost_dev *vdev;
	int lcore;

	printf(" Destroying the device ..... \n");
	dev->flags &= ~VIRTIO_DEV_RUNNING;

	vdev = (struct vhost_dev *)dev->priv;
	/*set the remove flag. */
	vdev->remove = 1;
	while(vdev->ready != DEVICE_SAFE_REMOVE) {
		rte_pause();
	}

	/* Search for entry to be removed from lcore ll */
	ll_lcore_dev_cur = lcore_info[vdev->coreid].lcore_ll->ll_root_used;
	while (ll_lcore_dev_cur != NULL) {
		if (ll_lcore_dev_cur->vdev == vdev) {
			break;
		} else {
			ll_lcore_dev_last = ll_lcore_dev_cur;
			ll_lcore_dev_cur = ll_lcore_dev_cur->next;
		}
	}

	if (ll_lcore_dev_cur == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to find the dev to be destroy.\n",
			dev->device_fh);
		return;
	}

	/* Search for entry to be removed from main ll */
	ll_main_dev_cur = ll_root_used;
	ll_main_dev_last = NULL;
	while (ll_main_dev_cur != NULL) {
		if (ll_main_dev_cur->vdev == vdev) {
			break;
		} else {
			ll_main_dev_last = ll_main_dev_cur;
			ll_main_dev_cur = ll_main_dev_cur->next;
		}
	}

	/* Remove entries from the lcore and main ll. */
	rm_data_ll_entry(&lcore_info[vdev->coreid].lcore_ll->ll_root_used, ll_lcore_dev_cur, ll_lcore_dev_last);
	rm_data_ll_entry(&ll_root_used, ll_main_dev_cur, ll_main_dev_last);

	/* Set the dev_removal_flag on each lcore. */
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		lcore_info[lcore].lcore_ll->dev_removal_flag = REQUEST_DEV_REMOVAL;
	}

	/*
	 * Once each core has set the dev_removal_flag to ACK_DEV_REMOVAL we can be sure that
	 * they can no longer access the device removed from the linked lists and that the devices
	 * are no longer in use.
	 */
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		while (lcore_info[lcore].lcore_ll->dev_removal_flag != ACK_DEV_REMOVAL) {
			rte_pause();
		}
	}

	/* Add the entries back to the lcore and main free ll.*/
	put_data_ll_free_entry(&lcore_info[vdev->coreid].lcore_ll->ll_root_free, ll_lcore_dev_cur);
	put_data_ll_free_entry(&ll_root_free, ll_main_dev_cur);

	/* Decrement number of device on the lcore. */
	lcore_info[vdev->coreid].lcore_ll->device_num--;

	RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") Device has been removed from data core\n", dev->device_fh);

	if (zero_copy) {
		struct vpool *vpool = &vpool_array[vdev->vmdq_rx_q];

		/* Stop the RX queue. */
		if (rte_eth_dev_rx_queue_stop(ports[0], vdev->vmdq_rx_q) != 0) {
			LOG_DEBUG(VHOST_CONFIG,
				"(%"PRIu64") In destroy_device: Failed to stop "
				"rx queue:%d\n",
				dev->device_fh,
				vdev->vmdq_rx_q);
		}

		LOG_DEBUG(VHOST_CONFIG,
			"(%"PRIu64") in destroy_device: Start put mbuf in "
			"mempool back to ring for RX queue: %d\n",
			dev->device_fh, vdev->vmdq_rx_q);

		mbuf_destroy_zcp(vpool);

		/* Stop the TX queue. */
		if (rte_eth_dev_tx_queue_stop(ports[0], vdev->vmdq_rx_q) != 0) {
			LOG_DEBUG(VHOST_CONFIG,
				"(%"PRIu64") In destroy_device: Failed to "
				"stop tx queue:%d\n",
				dev->device_fh, vdev->vmdq_rx_q);
		}

		vpool = &vpool_array[vdev->vmdq_rx_q + MAX_QUEUES];

		LOG_DEBUG(VHOST_CONFIG,
			"(%"PRIu64") destroy_device: Start put mbuf in mempool "
			"back to ring for TX queue: %d, dev:(%"PRIu64")\n",
			dev->device_fh, (vdev->vmdq_rx_q + MAX_QUEUES),
			dev->device_fh);

		mbuf_destroy_zcp(vpool);
		rte_free(vdev->regions_hpa);
	}
	rte_free(vdev);

}

/*
 * Calculate the region count of physical continous regions for one particular
 * region of whose vhost virtual address is continous. The particular region
 * start from vva_start, with size of 'size' in argument.
 */
static uint32_t
check_hpa_regions(uint64_t vva_start, uint64_t size)
{
	uint32_t i, nregions = 0, page_size = getpagesize();
	uint64_t cur_phys_addr = 0, next_phys_addr = 0;
	if (vva_start % page_size) {
		LOG_DEBUG(VHOST_CONFIG,
			"in check_countinous: vva start(%p) mod page_size(%d) "
			"has remainder\n",
			(void *)(uintptr_t)vva_start, page_size);
		return 0;
	}
	if (size % page_size) {
		LOG_DEBUG(VHOST_CONFIG,
			"in check_countinous: "
			"size((%"PRIu64")) mod page_size(%d) has remainder\n",
			size, page_size);
		return 0;
	}
	for (i = 0; i < size - page_size; i = i + page_size) {
		cur_phys_addr
			= rte_mem_virt2phy((void *)(uintptr_t)(vva_start + i));
		next_phys_addr = rte_mem_virt2phy(
			(void *)(uintptr_t)(vva_start + i + page_size));
		if ((cur_phys_addr + page_size) != next_phys_addr) {
			++nregions;
			LOG_DEBUG(VHOST_CONFIG,
				"in check_continuous: hva addr:(%p) is not "
				"continuous with hva addr:(%p), diff:%d\n",
				(void *)(uintptr_t)(vva_start + (uint64_t)i),
				(void *)(uintptr_t)(vva_start + (uint64_t)i
				+ page_size), page_size);
			LOG_DEBUG(VHOST_CONFIG,
				"in check_continuous: hpa addr:(%p) is not "
				"continuous with hpa addr:(%p), "
				"diff:(%"PRIu64")\n",
				(void *)(uintptr_t)cur_phys_addr,
				(void *)(uintptr_t)next_phys_addr,
				(next_phys_addr-cur_phys_addr));
		}
	}
	return nregions;
}

/*
 * Divide each region whose vhost virtual address is continous into a few
 * sub-regions, make sure the physical address within each sub-region are
 * continous. And fill offset(to GPA) and size etc. information of each
 * sub-region into regions_hpa.
 */
static uint32_t
fill_hpa_memory_regions(struct virtio_memory_regions_hpa *mem_region_hpa, struct virtio_memory *virtio_memory)
{
	uint32_t regionidx, regionidx_hpa = 0, i, k, page_size = getpagesize();
	uint64_t cur_phys_addr = 0, next_phys_addr = 0, vva_start;

	if (mem_region_hpa == NULL)
		return 0;

	for (regionidx = 0; regionidx < virtio_memory->nregions; regionidx++) {
		vva_start = virtio_memory->regions[regionidx].guest_phys_address +
			virtio_memory->regions[regionidx].address_offset;
		mem_region_hpa[regionidx_hpa].guest_phys_address
			= virtio_memory->regions[regionidx].guest_phys_address;
		mem_region_hpa[regionidx_hpa].host_phys_addr_offset =
			rte_mem_virt2phy((void *)(uintptr_t)(vva_start)) -
			mem_region_hpa[regionidx_hpa].guest_phys_address;
		LOG_DEBUG(VHOST_CONFIG,
			"in fill_hpa_regions: guest phys addr start[%d]:(%p)\n",
			regionidx_hpa,
			(void *)(uintptr_t)
			(mem_region_hpa[regionidx_hpa].guest_phys_address));
		LOG_DEBUG(VHOST_CONFIG,
			"in fill_hpa_regions: host  phys addr start[%d]:(%p)\n",
			regionidx_hpa,
			(void *)(uintptr_t)
			(mem_region_hpa[regionidx_hpa].host_phys_addr_offset));
		for (i = 0, k = 0;
			i < virtio_memory->regions[regionidx].memory_size -
				page_size;
			i += page_size) {
			cur_phys_addr = rte_mem_virt2phy(
					(void *)(uintptr_t)(vva_start + i));
			next_phys_addr = rte_mem_virt2phy(
					(void *)(uintptr_t)(vva_start +
					i + page_size));
			if ((cur_phys_addr + page_size) != next_phys_addr) {
				mem_region_hpa[regionidx_hpa].guest_phys_address_end =
					mem_region_hpa[regionidx_hpa].guest_phys_address +
					k + page_size;
				mem_region_hpa[regionidx_hpa].memory_size
					= k + page_size;
				LOG_DEBUG(VHOST_CONFIG, "in fill_hpa_regions: guest "
					"phys addr end  [%d]:(%p)\n",
					regionidx_hpa,
					(void *)(uintptr_t)
					(mem_region_hpa[regionidx_hpa].guest_phys_address_end));
				LOG_DEBUG(VHOST_CONFIG,
					"in fill_hpa_regions: guest phys addr "
					"size [%d]:(%p)\n",
					regionidx_hpa,
					(void *)(uintptr_t)
					(mem_region_hpa[regionidx_hpa].memory_size));
				mem_region_hpa[regionidx_hpa + 1].guest_phys_address
					= mem_region_hpa[regionidx_hpa].guest_phys_address_end;
				++regionidx_hpa;
				mem_region_hpa[regionidx_hpa].host_phys_addr_offset =
					next_phys_addr -
					mem_region_hpa[regionidx_hpa].guest_phys_address;
				LOG_DEBUG(VHOST_CONFIG, "in fill_hpa_regions: guest"
					" phys addr start[%d]:(%p)\n",
					regionidx_hpa,
					(void *)(uintptr_t)
					(mem_region_hpa[regionidx_hpa].guest_phys_address));
				LOG_DEBUG(VHOST_CONFIG,
					"in fill_hpa_regions: host  phys addr "
					"start[%d]:(%p)\n",
					regionidx_hpa,
					(void *)(uintptr_t)
					(mem_region_hpa[regionidx_hpa].host_phys_addr_offset));
				k = 0;
			} else {
				k += page_size;
			}
		}
		mem_region_hpa[regionidx_hpa].guest_phys_address_end
			= mem_region_hpa[regionidx_hpa].guest_phys_address
			+ k + page_size;
		mem_region_hpa[regionidx_hpa].memory_size = k + page_size;
		LOG_DEBUG(VHOST_CONFIG, "in fill_hpa_regions: guest phys addr end  "
			"[%d]:(%p)\n", regionidx_hpa,
			(void *)(uintptr_t)
			(mem_region_hpa[regionidx_hpa].guest_phys_address_end));
		LOG_DEBUG(VHOST_CONFIG, "in fill_hpa_regions: guest phys addr size "
			"[%d]:(%p)\n", regionidx_hpa,
			(void *)(uintptr_t)
			(mem_region_hpa[regionidx_hpa].memory_size));
		++regionidx_hpa;
	}
	return regionidx_hpa;
}

/*
 * A new device is added to a data core. First the device is added to the main linked list
 * and the allocated to a specific data core.
 */
static int
new_device (struct virtio_net *dev)
{
	struct virtio_net_data_ll *ll_dev;
	int lcore, core_add = 0;
	uint32_t device_num_min = num_devices;
	struct vhost_dev *vdev;
	uint32_t regionidx;

	vdev = rte_zmalloc("vhost device", sizeof(*vdev), RTE_CACHE_LINE_SIZE);
	if (vdev == NULL) {
		RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") Couldn't allocate memory for vhost dev\n",
			dev->device_fh);
		return -1;
	}
	vdev->dev = dev;
	dev->priv = vdev;

	if (zero_copy) {
		vdev->nregions_hpa = dev->mem->nregions;
		for (regionidx = 0; regionidx < dev->mem->nregions; regionidx++) {
			vdev->nregions_hpa
				+= check_hpa_regions(
					dev->mem->regions[regionidx].guest_phys_address
					+ dev->mem->regions[regionidx].address_offset,
					dev->mem->regions[regionidx].memory_size);

		}

		vdev->regions_hpa = rte_calloc("vhost hpa region",
					       vdev->nregions_hpa,
					       sizeof(struct virtio_memory_regions_hpa),
					       RTE_CACHE_LINE_SIZE);
		if (vdev->regions_hpa == NULL) {
			RTE_LOG(ERR, VHOST_CONFIG, "Cannot allocate memory for hpa region\n");
			rte_free(vdev);
			return -1;
		}


		if (fill_hpa_memory_regions(
			vdev->regions_hpa, dev->mem
			) != vdev->nregions_hpa) {

			RTE_LOG(ERR, VHOST_CONFIG,
				"hpa memory regions number mismatch: "
				"[%d]\n", vdev->nregions_hpa);
			rte_free(vdev->regions_hpa);
			rte_free(vdev);
			return -1;
		}
	}


	/* Add device to main ll */
	ll_dev = get_data_ll_free_entry(&ll_root_free);
	if (ll_dev == NULL) {
		RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") No free entry found in linked list. Device limit "
			"of %d devices per core has been reached\n",
			dev->device_fh, num_devices);
		if (vdev->regions_hpa)
			rte_free(vdev->regions_hpa);
		rte_free(vdev);
		return -1;
	}
	ll_dev->vdev = vdev;
	add_data_ll_entry(&ll_root_used, ll_dev);
	vdev->vmdq_rx_q
		= dev->device_fh * queues_per_pool + vmdq_queue_base;

	if (zero_copy) {
		uint32_t index = vdev->vmdq_rx_q;
		uint32_t count_in_ring, i;
		struct mbuf_table *tx_q;

		count_in_ring = rte_ring_count(vpool_array[index].ring);

		LOG_DEBUG(VHOST_CONFIG,
			"(%"PRIu64") in new_device: mbuf count in mempool "
			"before attach is: %d\n",
			dev->device_fh,
			rte_mempool_count(vpool_array[index].pool));
		LOG_DEBUG(VHOST_CONFIG,
			"(%"PRIu64") in new_device: mbuf count in  ring "
			"before attach  is : %d\n",
			dev->device_fh, count_in_ring);

		/*
		 * Attach all mbufs in vpool.ring and put back intovpool.pool.
		 */
		for (i = 0; i < count_in_ring; i++)
			attach_rxmbuf_zcp(dev);

		LOG_DEBUG(VHOST_CONFIG, "(%"PRIu64") in new_device: mbuf count in "
			"mempool after attach is: %d\n",
			dev->device_fh,
			rte_mempool_count(vpool_array[index].pool));
		LOG_DEBUG(VHOST_CONFIG, "(%"PRIu64") in new_device: mbuf count in "
			"ring after attach  is : %d\n",
			dev->device_fh,
			rte_ring_count(vpool_array[index].ring));

		tx_q = &tx_queue_zcp[(uint16_t)vdev->vmdq_rx_q];
		tx_q->txq_id = vdev->vmdq_rx_q;

		if (rte_eth_dev_tx_queue_start(ports[0], vdev->vmdq_rx_q) != 0) {
			struct vpool *vpool = &vpool_array[vdev->vmdq_rx_q];

			LOG_DEBUG(VHOST_CONFIG,
				"(%"PRIu64") In new_device: Failed to start "
				"tx queue:%d\n",
				dev->device_fh, vdev->vmdq_rx_q);

			mbuf_destroy_zcp(vpool);
			rte_free(vdev->regions_hpa);
			rte_free(vdev);
			return -1;
		}

		if (rte_eth_dev_rx_queue_start(ports[0], vdev->vmdq_rx_q) != 0) {
			struct vpool *vpool = &vpool_array[vdev->vmdq_rx_q];

			LOG_DEBUG(VHOST_CONFIG,
				"(%"PRIu64") In new_device: Failed to start "
				"rx queue:%d\n",
				dev->device_fh, vdev->vmdq_rx_q);

			/* Stop the TX queue. */
			if (rte_eth_dev_tx_queue_stop(ports[0],
				vdev->vmdq_rx_q) != 0) {
				LOG_DEBUG(VHOST_CONFIG,
					"(%"PRIu64") In new_device: Failed to "
					"stop tx queue:%d\n",
					dev->device_fh, vdev->vmdq_rx_q);
			}

			mbuf_destroy_zcp(vpool);
			rte_free(vdev->regions_hpa);
			rte_free(vdev);
			return -1;
		}

	}

	/*reset ready flag*/
	vdev->ready = DEVICE_MAC_LEARNING;
	vdev->remove = 0;
core_add = 1; /* jana */
	/* Find a suitable lcore to add the device. */
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		if (lcore_info[lcore].lcore_ll->device_num < device_num_min) {
			device_num_min = lcore_info[lcore].lcore_ll->device_num;
			core_add = lcore;
		}
	}
	/* Add device to lcore ll */
	ll_dev = get_data_ll_free_entry(&lcore_info[core_add].lcore_ll->ll_root_free);
	if (ll_dev == NULL) {
		RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") Failed to add device to data core\n", dev->device_fh);
		vdev->ready = DEVICE_SAFE_REMOVE;
		destroy_device(dev);
		if (vdev->regions_hpa)
			rte_free(vdev->regions_hpa);
		rte_free(vdev);
		return -1;
	}
	ll_dev->vdev = vdev;
	vdev->coreid = core_add;

	add_data_ll_entry(&lcore_info[vdev->coreid].lcore_ll->ll_root_used, ll_dev);

	/* Initialize device stats */
	memset(&dev_statistics[dev->device_fh], 0, sizeof(struct device_statistics));

	/* Disable notifications. */
	rte_vhost_enable_guest_notification(dev, VIRTIO_RXQ, 0);
	rte_vhost_enable_guest_notification(dev, VIRTIO_TXQ, 0);
	lcore_info[vdev->coreid].lcore_ll->device_num++;
	dev->flags |= VIRTIO_DEV_RUNNING;

	RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") Device has been added to data core %d\n", dev->device_fh, vdev->coreid);

	return 0;
}

/*
 * These callback allow devices to be added to the data core when configuration
 * has been fully complete.
 */
static const struct virtio_net_device_ops virtio_net_device_ops =
{
	.new_device =  new_device,
	.destroy_device = destroy_device,
};

/*
 * This is a thread will wake up after a period to print stats if the user has
 * enabled them.
 */
static void
print_stats(void)
{
	struct virtio_net_data_ll *dev_ll;
	uint64_t tx_dropped, rx_dropped;
	uint64_t tx, tx_total, rx, rx_total;
	uint32_t device_fh;
	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char top_left[] = { 27, '[', '1', ';', '1', 'H','\0' };

	while(1) {
		sleep(enable_stats);

		/* Clear screen and move to top left */
		printf("%s%s", clr, top_left);

		printf("\nDevice statistics ====================================");

		dev_ll = ll_root_used;
		while (dev_ll != NULL) {
			device_fh = (uint32_t)dev_ll->vdev->dev->device_fh;
			tx_total = dev_statistics[device_fh].tx_total;
			tx = dev_statistics[device_fh].tx;
			tx_dropped = tx_total - tx;
			if (zero_copy == 0) {
				rx_total = rte_atomic64_read(
					&dev_statistics[device_fh].rx_total_atomic);
				rx = rte_atomic64_read(
					&dev_statistics[device_fh].rx_atomic);
			} else {
				rx_total = dev_statistics[device_fh].rx_total;
				rx = dev_statistics[device_fh].rx;
			}
			rx_dropped = rx_total - rx;

			printf("\nStatistics for device %"PRIu32" ------------------------------"
					"\nTX total: 		%"PRIu64""
					"\nTX dropped: 		%"PRIu64""
					"\nTX successful: 		%"PRIu64""
					"\nRX total: 		%"PRIu64""
					"\nRX dropped: 		%"PRIu64""
					"\nRX successful: 		%"PRIu64"",
					device_fh,
					tx_total,
					tx_dropped,
					tx,
					rx_total,
					rx_dropped,
					rx);

			dev_ll = dev_ll->next;
		}
		printf("\n======================================================\n");
	}
}

/*
 * Main function, does initialisation and calls the per-lcore functions. The CUSE
 * device is also registered here to handle the IOCTLs.
 */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool = NULL;
	unsigned lcore_id, core_id = 0;
	unsigned nb_ports, valid_num_ports;
	int ret;
	uint8_t portid;
	uint16_t queue_id;
	static pthread_t tid;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	argc -= ret;
	argv += ret;

	/* parse app arguments */
	ret = us_vhost_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid argument\n");

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id ++)
		if (rte_lcore_is_enabled(lcore_id))
			lcore_ids[core_id ++] = lcore_id;

	if (rte_lcore_count() > RTE_MAX_LCORE)
		rte_exit(EXIT_FAILURE,"Not enough cores\n");

	/*set the number of swithcing cores available*/
	num_switching_cores = rte_lcore_count()-1;

	/* Get the number of physical ports. */
	nb_ports = rte_eth_dev_count();
	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

	/*
	 * Update the global var NUM_PORTS and global array PORTS
	 * and get value of var VALID_NUM_PORTS according to system ports number
	 */
	valid_num_ports = check_ports_num(nb_ports);

	if ((valid_num_ports ==  0) || (valid_num_ports > MAX_SUP_PORTS)) {
		RTE_LOG(INFO, VHOST_PORT, "Current enabled port number is %u,"
			"but only %u port can be enabled\n",num_ports, MAX_SUP_PORTS);
		return -1;
	}

	if (zero_copy == 0) {
		/* Create the mbuf pool. */
		mbuf_pool = rte_mempool_create(
				"MBUF_POOL",
				NUM_MBUFS_PER_PORT
				* valid_num_ports,
				MBUF_SIZE, MBUF_CACHE_SIZE,
				sizeof(struct rte_pktmbuf_pool_private),
				rte_pktmbuf_pool_init, NULL,
				rte_pktmbuf_init, NULL,
				rte_socket_id(), 0);
		if (mbuf_pool == NULL)
			rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

		for (queue_id = 0; queue_id < MAX_QUEUES + 1; queue_id++)
			vpool_array[queue_id].pool = mbuf_pool;

		if (vm2vm_mode == VM2VM_HARDWARE) {
			/* Enable VT loop back to let L2 switch to do it. */
			vmdq_conf_default.rx_adv_conf.vmdq_rx_conf.enable_loop_back = 1;
			LOG_DEBUG(VHOST_CONFIG,
				"Enable loop back for L2 switch in vmdq.\n");
		}
	} else {
		printf("ERROR ....: nut supported");
				return 0;
	}
	/* Set log level. */
	rte_set_log_level(LOG_LEVEL);

	/* initialize all ports */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			RTE_LOG(INFO, VHOST_PORT,
				"Skipping disabled port %d\n", portid);
			continue;
		}
		if (port_init(portid) != 0)
			rte_exit(EXIT_FAILURE,
				"Cannot initialize network ports\n");
	}

	/* Initialise all linked lists. */
	if (init_data_ll() == -1)
		rte_exit(EXIT_FAILURE, "Failed to initialize linked list\n");

	/* Initialize device stats */
	memset(&dev_statistics, 0, sizeof(dev_statistics));

	/* Enable stats if the user option is set. */
	if (enable_stats)
		pthread_create(&tid, NULL, (void*)print_stats, NULL );

	/* Launch all data cores. */
	if (zero_copy == 0) {
		RTE_LCORE_FOREACH_SLAVE(lcore_id) {
			rte_eal_remote_launch(switch_worker,
				mbuf_pool, lcore_id);
		}
	} else {
		printf("ERROR ....: nut supported");
		return 0;
	}

	if (mergeable == 0)
		rte_vhost_feature_disable(1ULL << VIRTIO_NET_F_MRG_RXBUF);

	/* Register CUSE device to handle IOCTLs. */
	ret = rte_vhost_driver_register((char *)&dev_basename);
	if (ret != 0)
		rte_exit(EXIT_FAILURE,"CUSE device setup failure.\n");

	rte_vhost_driver_callback_register(&virtio_net_device_ops);

	/* Start CUSE session. */
	rte_vhost_driver_session_start();
	return 0;

}


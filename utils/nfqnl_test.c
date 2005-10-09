
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <linux/netfilter.h>		/* for NF_ACCEPT */

#include <libnetfilter_queue/libnetfilter_queue.h>

/* returns packet id */
static u_int32_t print_pkt (struct nfattr *tb[])
{
	int id = 0;
	struct nfqnl_msg_packet_hdr *ph;
	u_int32_t mark,ifi; 
	int ret;
	unsigned int datalength;
	char * data;
	
	ph = nfqnl_get_msg_packet_hdr(tb);
	if (ph){
		id = ntohl(ph->packet_id);
		printf("hw_protocol=0x%04x hook=%u id=%u ",
			ntohs(ph->hw_protocol), ph->hook, id);
	}
	
	mark = nfqnl_get_nfmark(tb);
	if (mark)
		printf("mark=%u ", mark);

	ifi = nfqnl_get_indev(tb);
	if (ifi)
		printf("indev=%u ", ifi);

	ifi = nfqnl_get_outdev(tb);
	if (ifi)
		printf("outdev=%u ", ifi);

	ret = nfqnl_get_payload(tb, &data, &datalength);
	if (ret)
		printf("payload_len=%d ", datalength);

	fputc('\n', stdout);

	return id;
}
	

static int cb(struct nfqnl_q_handle *qh, struct nfgenmsg *nfmsg,
	      struct nfattr *nfa[], void *data)
{
	u_int32_t id = print_pkt(nfa);
	printf("entering callback\n");
	return nfqnl_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
}

int main(int argc, char **argv)
{
	struct nfqnl_handle *h;
	struct nfqnl_q_handle *qh;
	struct nfnl_handle *nh;
	int fd;
	int rv;
	char buf[4096];

	printf("opening library handle\n");
	h = nfqnl_open();
	if (!h) {
		fprintf(stderr, "error during nfqnl_open()\n");
		exit(1);
	}

	printf("unbinding existing nf_queue handler for AF_INET (if any)\n");
	if (nfqnl_unbind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "error during nfqnl_unbind_pf()\n");
		exit(1);
	}

	printf("binding nfnetlink_queue as nf_queue handler for AF_INET\n");
	if (nfqnl_bind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "error during nfqnl_bind_pf()\n");
		exit(1);
	}

	printf("binding this socket to queue '0'\n");
	qh = nfqnl_create_queue(h,  0, &cb, NULL);
	if (!qh) {
		fprintf(stderr, "error during nfqnl_create_queue()\n");
		exit(1);
	}

	printf("setting copy_packet mode\n");
	if (nfqnl_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) {
		fprintf(stderr, "can't set packet_copy mode\n");
		exit(1);
	}

	nh = nfqnl_nfnlh(h);
	fd = nfnl_fd(nh);

	while ((rv = recv(fd, buf, sizeof(buf), 0)) && rv >= 0) {
		printf("pkt received\n");
		nfqnl_handle_packet(h, buf, rv);
	}

	printf("unbinding from queue 0\n");
	nfqnl_destroy_queue(qh);

#ifdef INSANE
	/* normally, applications SHOULD NOT issue this command, since
	 * it detaches other programs/sockets from AF_INET, too ! */
	printf("unbinding from AF_INET\n");
	nfqnl_unbind_pf(h, AF_INET);
#endif

	printf("closing library handle\n");
	nfqnl_close(h);

	exit(0);
}

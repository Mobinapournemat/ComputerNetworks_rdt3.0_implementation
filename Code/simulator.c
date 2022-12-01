#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>  
#include <string.h>



/********************************************************* NOTE ********************************************************/
/*  sliding window in Go-Back_N  ---> N = 1 : bakhshe Alef  , N = 8 : bakhshe b , Bakhshe emtiazi ham zade shode  */

int N = 8; 



struct msg {
  char data[20];
  };



struct pkt {
   int seqnum;
   int acknum;
   int checksum;
   char payload[20];
    };


void starttimer(int calling_node, float increment);


void stoptimer(int calling_node);


void tolayer3(int calling_node,struct pkt packet);


void tolayer5(int calling_node, struct msg message);





/*
  in order to keet the order of packets, we can use a linked list. the existing struct for packets
  is not suitable because it doesn't contain a pointer to the next packet. so we should define 
  a new struct for packet nodes to use it in the linked list.
*/

struct node_pkt {
    struct pkt packet;
    struct node_pkt *next_pkt;
};

/*
  Moreover, we need a linked list object to keep track of nodes whithin our list.
*/
struct linked_list {
  int length;
  struct node_pkt *first_pkt;
};


/* because we use cumulative acks, when we receive an ack we should remove packets before that packet from no_acked packets list because we have their acks too. */
int remove_packets(int number, struct linked_list *packets_linkedlist) {
  struct node_pkt *node = packets_linkedlist->first_pkt;
  /* we should iterate the linkedlist from its first node until we get to the node which its seqnum equals to number */
  int counter = 1;
  if (node == NULL)
    return 0;
  while (true){
    if ((node->packet).seqnum == number){
      packets_linkedlist->first_pkt = node->next_pkt;
      packets_linkedlist->length -= counter;
      return counter;
    }
    node = node->next_pkt;
    counter += 1;
    if (node == NULL)
      return 0;
  }
}


/* to add a new packet node to the packets linkedlist we'll use this function */
void add_pkt_to_linkedlist(struct pkt packet, struct linked_list *packets_linkedlist) {
  if (packets_linkedlist->length > 0) {   /* if the linkedlist is not empty we should find the right place for the new packet based on its seqnum */
    struct node_pkt *before = NULL;
    struct node_pkt *next = packets_linkedlist->first_pkt;
    while (next != NULL && (next->packet).seqnum <= packet.seqnum) {
      if ((next->packet).seqnum == packet.seqnum) { /* it means that we already have this packet but its ack didn't receive by sender, so the sender sent it again */
        return;
      }
      before = next; 
      next = next->next_pkt;
    }
    struct node_pkt *new_node = (struct node_pkt *)malloc(sizeof(struct node_pkt));
    new_node->packet = packet;
    new_node->next_pkt = next;
    packets_linkedlist->length += 1;
    if (before != NULL) { /* this node is going to add beyond some nodes. so the node before it, should set its next parameter. */
      before->next_pkt = new_node;
    } else {              /* it means that this node should be the first node in the linkedlist. */
      packets_linkedlist->first_pkt = new_node;
    }
    return;
  } 
  /* if the linkedlist is empty, this node will not have the next node and it would be the first node in the linkedlist */
  packets_linkedlist->length += 1;
  struct node_pkt *new_node = (struct node_pkt *)malloc(sizeof(struct node_pkt));
  new_node->packet = packet;
  new_node->next_pkt = NULL;
  packets_linkedlist->first_pkt = new_node;
}



/* using this function, we can create a new ack */
struct pkt create_ack(int acknum) {
  struct pkt ack;
  ack.seqnum = 0;
  ack.acknum = acknum;
  ack.checksum = get_checksum_value(ack);
  return ack;
} 


/* to check corruption of packets we use this function to compare checksum values before and after entering the network. */
int get_checksum_value(struct pkt packet) {
  int checksum = 0;
  for(int i = 0; i < 20; i++)
    checksum += (int)packet.payload[i];
  checksum += (packet.acknum + packet.seqnum);
  return checksum;
}



struct Node {
    struct linked_list *sent_no_ack;
    struct linked_list *rec_no_ack;
} *A, *B;



int A_last_ack_num;
int A_last_seqnum;
/* called from layer 5, passed the data to be sent to other side. Return a 1 if 
data is accepted for transmission, negative number otherwise */
int A_output(message)
  struct msg message;
{
  struct msg *msg = &message;
  if (A->sent_no_ack->length == N) // it means that the buffer of packets is full
    return -1;
  struct pkt packet;  // create pkt structure from msg structure
  strcpy(packet.payload, msg->data);
  packet.acknum = 0;
  packet.seqnum = A_last_seqnum++;
  packet.checksum = get_checksum_value(packet);
  add_pkt_to_linkedlist(packet, A->sent_no_ack); // now we should add this packet to the list of packets that has been sent but its ack don't received
  tolayer3(0, packet); // send packet to the medium network
  if (A->sent_no_ack->length == 1)  // if there is a sent packet that its ack doesn't received yet, start the timer
    starttimer(0, 10);
  return 1;
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(packet)
  struct pkt packet;
{
  if (packet.checksum != get_checksum_value(packet) || (packet.seqnum > A_last_ack_num + 8)) // it means that packet is corrupted!
    return;
  if (packet.acknum != 0) { // it means that this packet is an ack
    int count_removed = remove_packets(packet.acknum, A->sent_no_ack);
    if (count_removed > 0) {
      stoptimer(0); // we received ack packet so we should stop the timer.
      if (A->sent_no_ack->length != 0) // if there is still some packets that we didn't received their ack, we should start the timer again!
        starttimer(0, 10);
    }
    return;
  }
  // of it's not an ack packet we should do following tasks: 
  if (A_last_ack_num >= packet.seqnum) { // it means that this packet is repititive but its ack doesn't received correctly by the sender. so we should not pass it to layer 5 but we should send its ack again!
    struct pkt ack = create_ack(A_last_ack_num);
    tolayer3(0, ack);
    return;
  }
  add_pkt_to_linkedlist(packet, A->rec_no_ack); // if this packet isn't repititive, we should add it to the list of received packets that their ack didn't sent.
  while ((A->rec_no_ack->first_pkt->packet).seqnum == A_last_ack_num+1) {
    struct node_pkt *node_ = A->rec_no_ack->first_pkt; // in order to send packets to layer 5, we should create corresponding msg
    A->rec_no_ack->first_pkt = node_->next_pkt;
    if (A->rec_no_ack->first_pkt == NULL)
      return;
    struct pkt pkt_ = node_->packet; 
    struct pkt ack_packet = create_ack(pkt_.seqnum); // we should send the ack to the sender
    tolayer3(0, ack_packet);
    A_last_ack_num += 1;
    struct msg message;
    strcpy(message.data, pkt_.payload);
    tolayer5(0, message);
  }
}


/* called when A's timer goes off */
void A_timerinterrupt()
{
  struct node_pkt *pkt_n = A->sent_no_ack->first_pkt;
  while (pkt_n != NULL) {   // because timer goes off, we should retransmit packets that their acks didn't received.
    tolayer3(0, pkt_n->packet);
    pkt_n = pkt_n->next_pkt;
  }
  if (A->sent_no_ack->length != 0) { // if there is stil packets in this list, we should start this timer again!
    starttimer(0, 10);
  }

}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
  struct Node *node = (struct Node *)malloc(sizeof(struct Node));
  A_last_ack_num = 0;
  A_last_seqnum = 1;
  struct linked_list *no_ack_send = (struct linked_list *)malloc(sizeof(struct linked_list));
  no_ack_send->length = 0;
  no_ack_send->first_pkt = NULL;
  node->sent_no_ack = no_ack_send;
  struct linked_list *no_ack_rec = (struct linked_list *)malloc(sizeof(struct linked_list));
  no_ack_rec->length = 0;
  no_ack_rec->first_pkt = NULL;
  node->rec_no_ack = no_ack_rec;
  A = node;
}


int B_last_ack_num;
int B_last_seqnum;

/* use only for biderectional data transfer */
int B_output(message) 
  struct msg *message;
{
  if (B->sent_no_ack->length == N) // it means that the buffer of packets is full
    return -1;
  struct pkt packet;  // create pkt structure from msg structure
  strcpy(packet.payload, message->data);
  packet.acknum = 0;
  packet.seqnum = B_last_seqnum += 1;
  packet.checksum = get_checksum_value(packet);
  add_pkt_to_linkedlist(packet, B->sent_no_ack); // now we should add this packet to the list of packets that has been sent but its ack don't received
  tolayer3(1, packet); // send packet to the medium network
  if (B->sent_no_ack->length == 1)  // if there is a sent packet that its ack doesn't received yet, start the timer
    starttimer(1, 10);
  return 1;
}


/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(packet)
  struct pkt packet;
{
    if (packet.checksum != get_checksum_value(packet) || (packet.seqnum > B_last_ack_num + 8)) // it means that packet is corrupted!
    return;
  if (packet.acknum != 0) { // it means that this packet is an ack
    int count_removed = remove_packets(packet.acknum, B->sent_no_ack);
    if (count_removed > 0) {
      stoptimer(1); // we received ack packet so we should stop the timer.
      if (B->sent_no_ack->length != 0) // if there is still some packets that we didn't received their ack, we should start the timer again!
        starttimer(1, 10);
    }
    return;
  }
  // of it's not an ack packet we should do following tasks: 
  if (B_last_ack_num >= packet.seqnum) { // it means that this packet is repititive but its ack doesn't received correctly by the sender. so we should not pass it to layer 5 but we should send its ack again!
    struct pkt ack = create_ack(B_last_ack_num);
    tolayer3(1, ack);
    return;
  }
  add_pkt_to_linkedlist(packet, B->rec_no_ack); // if this packet isn't repititive, we should add it to the list of received packets that their ack didn't sent.
  while ((B->rec_no_ack->first_pkt->packet).seqnum == B_last_ack_num+1) {
    struct node_pkt *node_ = B->rec_no_ack->first_pkt; // in order to send packets to layer 5, we should create corresponding msg
    B->rec_no_ack->first_pkt = node_->next_pkt;
    if (B->rec_no_ack->first_pkt == NULL)
      return;
    struct pkt pkt_ = node_->packet; 
    struct pkt ack_packet = create_ack(pkt_.seqnum); // we should send the ack to the sender
    tolayer3(1, ack_packet);
    B_last_ack_num += 1;
    struct msg message;
    strcpy(message.data, pkt_.payload);
    tolayer5(1, message);
  }

}

/* called when B's timer goes off */
void B_timerinterrupt()
{
  struct node_pkt *pkt_n = B->sent_no_ack->first_pkt;
  while (pkt_n != NULL) {   // because timer goes off, we should retransmit packets that their acks didn't received.
    tolayer3(1, pkt_n->packet);
    pkt_n = pkt_n->next_pkt;
  }
  if (B->sent_no_ack->length != 0) { // if there is stil packets in this list, we should start this timer again!
    starttimer(1, 10);
  }
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
  struct Node *node = (struct Node *)malloc(sizeof(struct Node));
  B_last_ack_num = 0;
  B_last_seqnum = 1;
  struct linked_list *no_ack_send = (struct linked_list *)malloc(sizeof(struct linked_list));
  no_ack_send->length = 0;
  no_ack_send->first_pkt = NULL;
  node->sent_no_ack = no_ack_send;
  struct linked_list *no_ack_rec = (struct linked_list *)malloc(sizeof(struct linked_list));
  no_ack_rec->length = 0;
  no_ack_rec->first_pkt = NULL;
  node->rec_no_ack = no_ack_rec;
  B = node;
}


/****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
    interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how the emulator 
designed , you're welcome to look at the code - but again, you should have
to, and you definitely should not have to modify anything.
******************************************************************/

struct event {
   float evtime;           /* event time */
   int evtype;             /* event type code */
   int eventity;           /* entity where event occurs */
   struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
   struct event *prev;
   struct event *next;
 };
struct event *evlist = NULL;   /* the event list */

/* use for bidirectional transfer of data */
#define BIDIRECTIONAL 0 

/* possible events: */
#define  TIMER_INTERRUPT 0  
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define   A    0
#define   B    1

int TRACE = 1;             /* for my debugging */
int nsim = 0;              /* number of messages from layer 5 to 4 so far */ 
int nsimmax = 0;           /* number of msgs to generate, then stop */
float simul_time = 0.000;        /* global simulation simul_time */
float lossprob;            /* probability that a packet is dropped  */
float corruptprob;         /* probability that one bit is packet is flipped */
float lambda;              /* arrival rate of messages from layer 5 */   
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int   ncorrupt;            /* number corrupted by media*/
int   randseed;            /* random number seed */





/****************** EVENT LIST ROUTINE  *************/
/* Event list manipulation routines                 */
/****************************************************/
void insertevent(p)
   struct event *p;
{
   struct event *q,*qold;

   if (TRACE>2) {
      printf("            INSERTEVENT: time is %lf\n",simul_time);
      printf("            INSERTEVENT: future time will be %lf\n",p->evtime); 
      }
   q = evlist;     /* q points to header of list in which p struct inserted */
   if (q==NULL) {   /* list is empty */
        evlist=p;
        p->next=NULL;
        p->prev=NULL;
        }
     else {
        for (qold = q; q !=NULL && p->evtime > q->evtime; q=q->next)
              qold=q; 
        if (q==NULL) {   /* end of list */
             qold->next = p;
             p->prev = qold;
             p->next = NULL;
             }
           else if (q==evlist) { /* front of list */
             p->next=evlist;
             p->prev=NULL;
             p->next->prev=p;
             evlist = p;
             }
           else {     /* middle of list */
             p->next=q;
             p->prev=q->prev;
             q->prev->next=p;
             q->prev=p;
             }
         }
}

void printevlist()
{
  struct event *q;
  printf("--------------\nEvent List Follows:\n");
  for(q = evlist; q!=NULL; q=q->next) {
    printf("Event time: %f, type: %d entity: %d\n",q->evtime,q->evtype,q->eventity);
    }
  printf("--------------\n");
}



/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void stoptimer(AorB)
int AorB;  /* A or B is trying to stop timer */
{
 struct event *q;

 if (TRACE>2)
    printf("          STOP TIMER: stopping timer at %f\n",simul_time);
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
 for (q=evlist; q!=NULL ; q = q->next) 
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) { 
       /* remove this event */
       if (q->next==NULL && q->prev==NULL)
             evlist=NULL;         /* remove first and only event on list */
          else if (q->next==NULL) /* end of list - there is one in front */
             q->prev->next = NULL;
          else if (q==evlist) { /* front of list - there must be event after */
             q->next->prev=NULL;
             evlist = q->next;
             }
           else {     /* middle of list */
             q->next->prev = q->prev;
             q->prev->next =  q->next;
             }
       free(q);
       return;
     }
  printf("Warning: unable to cancel your timer. It wasn't running.\n");
}


void starttimer(AorB,increment)
int AorB;  /* A or B is trying to stop timer */
float increment;
{

 struct event *q;
 struct event *evptr;

 if (TRACE>2)
    printf("          START TIMER: starting timer at %f\n",simul_time);
 /* be nice: check to see if timer is already started, if so, then  warn */
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
   for (q=evlist; q!=NULL ; q = q->next)  
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) { 
      printf("Warning: attempt to start a timer that is already started\n");
      return;
      }
 
/* create future event for when timer goes off */
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  simul_time + increment;
   evptr->evtype =  TIMER_INTERRUPT;
   evptr->eventity = AorB;
   insertevent(evptr);
} 


/************************** TOLAYER3 ***************/
double random_number() {

  // generate a uniform random number in the interval [0,1)
  return (double)1.0*rand()/(RAND_MAX+1.0);
}

void init_random(unsigned int randseed) {
  
  // initialize the random number generator
  if (!randseed) {
    srand((unsigned int)time(NULL));
  } else
    srand(randseed);
}

void tolayer3(AorB,packet)
int AorB;  /* A or B is trying to stop timer */
struct pkt packet;
{
 struct pkt *mypktptr;
 struct event *evptr,*q;
 float lastime, x;
 int i;

 ntolayer3++;

 /* simulate losses: */
 if (random_number() < lossprob)  {
      nlost++;
      if (TRACE>0)    
	printf("          TOLAYER3: packet being lost\n");
      return;
 }  

/* make a copy of the packet student just gave me since he/she may decide */
/* to do something with the packet after we return back to him/her */ 
 mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
 mypktptr->seqnum = packet.seqnum;
 mypktptr->acknum = packet.acknum;
 mypktptr->checksum = packet.checksum;
 for (i=0; i<20; i++)
    mypktptr->payload[i] = packet.payload[i];
 if (TRACE>2)  {
   printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
	  mypktptr->acknum,  mypktptr->checksum);
    for (i=0; i<20; i++)
        printf("%c",mypktptr->payload[i]);
    printf("\n");
   }

/* create future event for arrival of packet at the other side */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtype =  FROM_LAYER3;   /* packet will pop out from layer3 */
  evptr->eventity = (AorB+1) % 2; /* event occurs at other entity */
  evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */
/* finally, compute the arrival time of packet at the other end.
   medium can not reorder, so make sure packet arrives between 1 and 10
   time units after the latest arrival time of packets
   currently in the medium on their way to the destination */
 lastime = simul_time;
 for (q=evlist; q!=NULL ; q = q->next) 
    if ( (q->evtype==FROM_LAYER3  && q->eventity==evptr->eventity) ) 
      lastime = q->evtime;
 evptr->evtime =  lastime + 1.0 + 9.0*random_number();
 

 /* simulate corruption: */
 if (random_number() < corruptprob)  {
    ncorrupt++;
    if ( (x = random_number()) < .75)
       mypktptr->payload[0]='Z';   /* corrupt payload */
      else if (x < .875)
       mypktptr->seqnum = 999999;
      else
       mypktptr->acknum = 999999;
    if (TRACE>0)    
	printf("          TOLAYER3: packet being corrupted\n");
    }  

  if (TRACE>2)  
     printf("          TOLAYER3: scheduling arrival on other side\n");
  insertevent(evptr);
} 

void tolayer5(AorB, msgReceived)
  int AorB;
  struct msg msgReceived;
{
  int i;  
  if (TRACE>2) {
     printf("          TOLAYER5: data received: ");
     for (i=0; i<20; i++)  
        printf("%c",msgReceived.data[i]);
     printf("\n");
  }
  
}

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/
 
void generate_next_arrival(int entity)
{
   double x;
   struct event *evptr;

   if (TRACE>2)
       printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");
 
   x = lambda*random_number()*2;   /* x is uniform on [0,2*lambda] */
                             /* having mean of lambda        */
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  simul_time + x;
   evptr->evtype =  FROM_LAYER5;

   if (entity) 
     evptr->eventity = entity;
   else {
     if (BIDIRECTIONAL && (random_number()>0.5) )
       evptr->eventity = B;
     else
       evptr->eventity = A;
   }
   insertevent(evptr);
} 


/*************** INITIALIZATION ROUTINE  *************/
/* Read input from user and initalize parameters     */
/*****************************************************/
void init()                         
{
   int i;
   float sum, avg;

   printf("-----  Stop and Wait Network Simulator -------- \n\n");
   printf("Enter the number of messages to simulate: ");
   scanf("%d",&nsimmax);
   printf("Enter packet loss probability [enter 0.0 for no loss]: ");
   scanf("%f",&lossprob);
   printf("Enter packet corruption probability [0.0 for no corruption]: ");
   scanf("%f",&corruptprob);
   printf("Enter average time between messages from sender's layer5 [ > 0.0]: ");
   scanf("%f",&lambda);
   printf("Enter a seed for the random number generator [0 will provide a random seed]: ");
   scanf("%d",&randseed);
   printf("Enter TRACE [0,1,2,3]: ");
   scanf("%d",&TRACE);

   /* init random number generator */
   init_random(randseed);

   sum = 0.0;                /* test random number generator for students */
   for (i=0; i<1000; i++)
      sum=sum+random_number();     /* should be uniform in [0,1) */
   avg = sum/1000.0;
   if ((avg < 0.25) || (avg > 0.75)) {
    printf("It is likely that random number generation on your machine\n" ); 
    printf("is different from what this emulator expects.  Please take\n");
    printf("a look at the routine random_number() in the emulator code. Sorry. \n");
    exit(0);
   }

   ntolayer3 = 0;
   nlost = 0;
   ncorrupt = 0;

   simul_time=0.0;                     /* initialize simul_time to 0.0 */
   generate_next_arrival(0);     /* initialize event list */
}

/********************* MAIN ROUTINE  *****************/
/* Main simulation loop and handling of events       */
/*****************************************************/
int main(void)
{
   struct event *eventptr;
   struct msg  msg2give;
   struct pkt  pkt2give;
   
   int i,j;

   /* initialize our data structures and read parameters */
   init();

   /* call the user's init functions */
   A_init();
   B_init();
   
   /* loop forever... */
   while (1) {
        eventptr = evlist;            /* get next event to simulate */
        if (eventptr==NULL) {
	  printf("INTERNAL PANIC: Event list is empty! This should NOT have happened.\n");
	  break;
	}
        evlist = evlist->next;        /* remove this event from event list */
        if (evlist!=NULL)
           evlist->prev=NULL;
        if (TRACE>=2) {
           printf("\nEVENT time: %f,",eventptr->evtime);
           printf("  type: %d",eventptr->evtype);
           if (eventptr->evtype==0)
	       printf(", timerinterrupt  ");
             else if (eventptr->evtype==1)
               printf(", fromlayer5 ");
             else
	     printf(", fromlayer3 ");
           printf(" entity: %d\n",eventptr->eventity);
           }
        simul_time = eventptr->evtime;        /* update simul_time to next event time */
        if (nsim==nsimmax)
	  break;                        /* all done with simulation */
        if (eventptr->evtype == FROM_LAYER5 ) {

            /* fill in msg to give with string of same letter */    
            j = nsim % 26; 
            for (i=0; i<20; i++)  
               msg2give.data[i] = 97 + j;
            if (TRACE>2) {
               printf("          MAINLOOP: data given to student: ");
                 for (i=0; i<20; i++) 
                  printf("%c", msg2give.data[i]);
               printf("\n");
	    }

            if (eventptr->eventity == A) 
               j = A_output(msg2give);
             else
               j = B_output(msg2give);
           
	    if (j < 0) {
	      if (TRACE>=1)
		printf("          MAINLOOP: data NOT accepted by layer 4 (student code)\n");
	      /* set up future arrival for the same entity*/
	      generate_next_arrival(eventptr->eventity);   

	    } else {
	      nsim++;
	      if (TRACE>=1)
		printf("          MAINLOOP: data accepted by layer 4 (student code)\n");
	      /* set up future arrival */
	      generate_next_arrival(0);   
	    }
	}
          else if (eventptr->evtype ==  FROM_LAYER3) {
            pkt2give.seqnum = eventptr->pktptr->seqnum;
            pkt2give.acknum = eventptr->pktptr->acknum;
            pkt2give.checksum = eventptr->pktptr->checksum;
            for (i=0; i<20; i++)  
                pkt2give.payload[i] = eventptr->pktptr->payload[i];
	    if (eventptr->eventity == A)      /* deliver packet by calling */
   	       A_input(pkt2give);             /* appropriate entity */
            else
   	       B_input(pkt2give);
	    free(eventptr->pktptr);          /* free the memory for packet */
            }
          else if (eventptr->evtype ==  TIMER_INTERRUPT) {
            if (eventptr->eventity == A) 
	       A_timerinterrupt();
             else
	       B_timerinterrupt();
             }
          else  {
	     printf("INTERNAL PANIC: unknown event type \n");
	     break;
	  }
        free(eventptr);
   }

   printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n",simul_time,nsim);
   return 0;
}


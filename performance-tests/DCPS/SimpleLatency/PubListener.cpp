// -*- C++ -*-
//
// $Id$
#include "PubListener.h"
#include "PubMessageTypeSupportImpl.h"
#include "AckMessageTypeSupportImpl.h"
#include "PubMessageTypeSupportC.h"
#include "AckMessageTypeSupportC.h"

#include <dds/DCPS/Service_Participant.h>
#include <ace/streams.h>


#include <time.h>
#include <math.h>
//#include <sys/time.h>

//#include "tester.h"



typedef struct
{
    char name[20];
    ACE_hrtime_t average;
    ACE_hrtime_t min;
    ACE_hrtime_t max;
    ACE_hrtime_t sum;
    ACE_hrtime_t sum2;
    int count;
} stats_type;

//
// Static functions
//

static void
add_stats (
    stats_type& stats,
    ACE_hrtime_t data
    )
{
  data = data / (ACE_hrtime_t) 1000;
    std::cout << data << std::endl;
    stats.average = (stats.count * stats.average + data)/(stats.count + 1);
    stats.min     = (stats.count == 0 || data < stats.min) ? data : stats.min;
    stats.max     = (stats.count == 0 || data > stats.max) ? data : stats.max;
    stats.sum = stats.sum + data;
    stats.sum2 = stats.sum2 + data * data;
    stats.count++;
}

static void
init_stats (
    stats_type& stats,
    char *name)
{
    strncpy ((char *)stats.name, name, 19);
    stats.name[19] = '\0';
    stats.count    = 0;
    stats.average  = 0.0;
    stats.min      = 0.0;
    stats.max      = 0.0;
    stats.sum      = 0.0;
    stats.sum2     = 0.0;
}

static double
std_dev (stats_type& stats)
{
  if (stats.count >=2)
  {
    return sqrt ((static_cast<double>(stats.count) * stats.sum2 - stats.sum * stats.sum) / 
                (static_cast<double>(stats.count) * static_cast<double>(stats.count - 1)));
  }
  return 0.0;
}




static stats_type round_trip;
//struct timeval round_pre_t;
//struct timeval round_post_t;


/* Defines number of warm-up samples which are used to avoid cold start issues */
#define TOTAL_PRIMER_SAMPLES      500
extern long total_samples;

// Implementation skeleton constructor
AckDataReaderListenerImpl::AckDataReaderListenerImpl(CORBA::Long size)
  :writer_ (),
   reader_ (),
   dr_servant_ (0),
   dw_servant_ (0),
   handle_ (),
   size_ (size),
   sample_num_(1),
   done_ (0)
{
  //
  // init timing statistics 
  //
  init_stats (round_trip, "round_trip");

}

// Implementation skeleton destructor
AckDataReaderListenerImpl::~AckDataReaderListenerImpl ()
{
}

void AckDataReaderListenerImpl::init(DDS::DataReader_ptr dr,
                                    DDS::DataWriter_ptr dw)
{
  this->writer_ = DDS::DataWriter::_duplicate (dw);
  this->reader_ = DDS::DataReader::_duplicate (dr);

  AckMessageDataReader_var ackmessage_dr = 
    AckMessageDataReader::_narrow(this->reader_.in());
  this->dr_servant_ =
    reference_to_servant< AckMessageDataReaderImpl,
                          AckMessageDataReader_ptr>(ackmessage_dr.in());

  PubMessageDataWriter_var pubmessage_dw =
    PubMessageDataWriter::_narrow (this->writer_.in ());
  this->dw_servant_ =
    reference_to_servant< PubMessageDataWriterImpl,
                          PubMessageDataWriter_ptr>(pubmessage_dw.in());
  DDSPerfTest::PubMessage msg;
  this->handle_ = this->dw_servant_->_cxx_register (msg);
}


void AckDataReaderListenerImpl::on_data_available(DDS::DataReader_ptr reader)
  throw (CORBA::SystemException)
{
    static DDSPerfTest::AckMessage message;
    static DDS::SampleInfo si;
    DDS::ReturnCode_t status = this->dr_servant_->take_next_sample(message, si) ;

    timer_.stop();

    CORBA::Long sequence_number = message.seqnum;


    if (status == DDS::RETCODE_OK) {
//      cout << "AckMessage: seqnum    = " << message.seqnum << endl;
    } else if (status == DDS::RETCODE_NO_DATA) {
      cerr << "ERROR: reader received DDS::RETCODE_NO_DATA!" << endl;
    } else {
      cerr << "ERROR: read Message: Error: " <<  status << endl;
    }

    if (sequence_number != this->sample_num_)
    {
      fprintf(stderr, 
              "ERROR - TAO_Pub: recieved seqnum %d on %d\n",
              sequence_number, this->sample_num_);

//      exit (1);
    }

    if (this->sample_num_ > TOTAL_PRIMER_SAMPLES)
    {
      ACE_hrtime_t round_trip_time;
      timer_.elapsed_time(round_trip_time);

      add_stats (round_trip, round_trip_time);
    }

    if (this->sample_num_ ==  total_samples + TOTAL_PRIMER_SAMPLES)
    {


      time_t clock = time (NULL);
      std::cout << "# MY Pub Sub measurements (in us) \n";
      std::cout << "# Executed at:" <<  ctime(&clock);
      std::cout << "#       Roundtrip time [us]\n";
      std::cout << "Count     mean      min      max   std_dev\n";
      std::cout << " "
                << round_trip.count
                << "        "
                << round_trip.average
                << "     "
                << round_trip.min
                << "      "
                << round_trip.max
                << "      "
                << std_dev (round_trip)
                << std::endl;


      DDSPerfTest::PubMessage msg;
      // send 0 to end the ping-pong operation
      msg.seqnum = 0;
      this->dw_servant_->write (msg, this->handle_);
      this->done_ = 1;
      return;
    }
    
    this->sample_num_++;
    
    DDSPerfTest::PubMessage msg;
    msg.seqnum = this->sample_num_;

    timer_.reset();
    timer_.start();
    this->dw_servant_->write (msg, this->handle_);
    
    return;    
}

void AckDataReaderListenerImpl::on_requested_deadline_missed (
    DDS::DataReader_ptr,
    const DDS::RequestedDeadlineMissedStatus &)
  throw (CORBA::SystemException)
{
}

void AckDataReaderListenerImpl::on_requested_incompatible_qos (
    DDS::DataReader_ptr,
    const DDS::RequestedIncompatibleQosStatus &)
  throw (CORBA::SystemException)
{
}

void AckDataReaderListenerImpl::on_liveliness_changed (
    DDS::DataReader_ptr,
    const DDS::LivelinessChangedStatus &)
  throw (CORBA::SystemException)
{
}

void AckDataReaderListenerImpl::on_subscription_match (
    DDS::DataReader_ptr,
    const DDS::SubscriptionMatchStatus &)
  throw (CORBA::SystemException)
{
}

void AckDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr,
    const DDS::SampleRejectedStatus&)
  throw (CORBA::SystemException)
{
}

void AckDataReaderListenerImpl::on_sample_lost(
  DDS::DataReader_ptr,
  const DDS::SampleLostStatus&)
  throw (CORBA::SystemException)
{
}

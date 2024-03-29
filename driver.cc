#include <fstream>
#include <iostream>
#include <sstream>
#include <uPRNG.h>

#include "config.h"
#include "printer.h"
#include "nameServer.h"
#include "bank.h"
#include "watcardOffice.h"
#include "groupoff.h"
#include "parent.h"
#include "student.h"
#include "vendingMachine.h"
#include "bottlingPlant.h"
#include <string>

using namespace std;

static intmax_t convert( const char * str ) {			// convert C string to integer
	char * endptr;
	errno = 0;											// reset
	intmax_t val = strtoll( str, &endptr, 10 );			// attempt conversion
	if ( errno == ERANGE ) throw std::out_of_range("");
	if ( endptr == str ||								// conversion failed, no characters generated
		 *endptr != '\0' ) throw std::invalid_argument(""); // not at end of str ?
	return val;
} // convert

int main( int argc, char * argv[] ) {
	string config = "soda.config";
	intmax_t seed, processors = 1;				// defaults

	try {
		switch ( argc ) {
          case 4: if ( strcmp( argv[3], "d" ) != 0 ) { processors = convert( argv[3] ); if ( processors <= 0 ) throw 1; }
		  case 3: if ( strcmp( argv[2], "d" ) != 0 ) { seed = convert( argv[2] ); if ( seed <= 0 ) throw 1; else set_seed(seed); }
		  case 2:  if ( strcmp( argv[1], "d" ) != 0 ) { config = argv[1]; }
		  case 1: break;								// use defaults
		  default: throw 1;
		} // switch
	} catch( ... ) {
		cout << "Usage: " << argv[0] << " [ config-file | 'd' [ seed (> 0) | 'd'  [ processors (> 0)| 'd' ] ] ]" << endl;
		exit( 1 );
	} // try

	ConfigParms cparms;

	processConfigFile(config.c_str(), cparms);

	Printer prt(cparms.numStudents, cparms.numVendingMachines, cparms.numCouriers);

	uProcessor p[processors - 1] __attribute__(( unused )); // create more kernel threads
    {
		NameServer * nameServer = new NameServer(prt, cparms.numVendingMachines, cparms.numStudents);
		Bank bank(cparms.numStudents);
		BottlingPlant * plant = new BottlingPlant(prt, *nameServer, cparms.numVendingMachines, cparms.maxShippedPerFlavour, cparms.maxStockPerFlavour, cparms.timeBetweenShipments);
		WATCardOffice * cardOffice = new WATCardOffice(prt, bank, cparms.numCouriers);
		Groupoff groupoff(prt, cparms.numStudents, cparms.sodaCost, cparms.groupoffDelay);

		Parent * parent = new Parent(prt, bank, cparms.numStudents, cparms.parentalDelay);

		Student* students[cparms.numStudents];
		VendingMachine* machines[cparms.numVendingMachines];

		for (unsigned int i = 0; i < cparms.numVendingMachines; i += 1) {
			machines[i] = new VendingMachine(prt, *nameServer, i, cparms.sodaCost);
		}

		for (unsigned int i = 0; i < cparms.numStudents; i += 1) {
			students[i] = new Student(prt, *nameServer, *cardOffice, groupoff, i, cparms.maxPurchases);
		}

		for (unsigned int i = 0; i < cparms.numStudents; i += 1) delete students[i];

		// truck should be deleted ahead of vending machines
		// Otherwise:
		// uC++ Runtime error (UNIX pid:18271) (uSerial &)0x5624323022d0 : Entry failure while executing mutex destructor: mutex object has been destroyed.
		// Error occurred while executing task Truck (0x562432342440).

		delete plant;	

		for (unsigned int i = 0; i < cparms.numVendingMachines; i += 1) {
			delete machines[i];
		}		

		delete nameServer;

		delete cardOffice;

		delete parent;
    }
} // main

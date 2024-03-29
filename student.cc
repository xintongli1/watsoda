#include <uPRNG.h>
#include "student.h"
#include "groupoff.h"
#include "watcardOffice.h"
#include "vendingMachine.h"
#include "nameServer.h"
#include "printer.h"

Student::Student( Printer & prt, NameServer & nameServer, WATCardOffice & cardOffice, Groupoff & groupoff,
                    unsigned int id, unsigned int maxPurchases ) : prt(prt), nameServer(nameServer), cardOffice(cardOffice), groupoff(groupoff), id(id), maxPurchases(maxPurchases) {
    // select number of bottles to purchase
    amount = ::prng(1, maxPurchases);

    // select a favourite flavour
    flavour = (BottlingPlant::Flavours)::prng(0, 3);
    prt.print(Printer::Kind::Student, id, 'S', flavour, amount);

    // create WATCard with $5 balance
    card = cardOffice.create(id, 5);

    // create gift card with value of SodaCost
    giftCard = groupoff.giftCard();

    // find vending machine location
    vm = nameServer.getMachine(id);
    prt.print(Printer::Kind::Student, id, 'V', (int)(vm->getId()));
}

Student::~Student() {
    // free memory
    if (thisCard) { delete thisCard; }
    else {  // watcard not created
        try {
            _Select ( card ) {  // wait for creation before deletion
                delete card();
            }
        } catch ( WATCardOffice::Lost & ) {
            // new watcard deleted in courier
        } 
    }
}

void Student::main() {
    for ( unsigned int purchased = 0; purchased < amount; purchased += 1 ) {
        // yield random number 1 - 10 to decide next purchase
        yield(::prng(1, 10));

        for (;;) {
            unsigned int sodaCost = vm->cost();

            // wait until watcard or giftcard is available
            _Select ( card || giftCard );

            if ( giftCard.available() && giftCard()->getBalance() >= sodaCost) { // use giftcard
                try {
                    // purchase (Use gift card first if it was not used)
                    vm->buy(flavour, *giftCard());
                    prt.print(Printer::Kind::Student, id, 'G', flavour, giftCard()->getBalance());
                    giftCard.reset();   // to prevent further usage  
                    break;  
                } catch ( VendingMachine::Free & ) {
                //  must watch an ad
                    yield(4);
                    prt.print(Printer::Kind::Student, id, 'a', flavour, giftCard()->getBalance());
                    break;
                } catch (VendingMachine::Stock &) {
                    // out of stock change machine and try again
                    vm = nameServer.getMachine(id);
                    prt.print(Printer::Kind::Student, id, 'V', vm->getId());
                } // try
            } // if (giftcard ...)
            else if (card.available()){   // use watcard
                try {   // ignore watcard creation throwing WATCardOffice::Lost & 
                    thisCard = card();  
                    try {   // watcard creation is successful
                        vm->buy(flavour, *thisCard);
                        prt.print(Printer::Kind::Student, id, 'B', flavour, card()->getBalance());
                        break;
                    } catch ( VendingMachine::Free & ) {
                        //  must watch an ad
                        yield(4);
                        prt.print(Printer::Kind::Student, id, 'A', flavour, card()->getBalance());
                        break;
                    } catch ( VendingMachine::Funds & ) {
                        card.reset();
                        // insufficient funds
                        card = cardOffice.transfer(id, 5 + sodaCost, thisCard);
                    } catch (VendingMachine::Stock &) {
                        // out of stock change machine and try again
                        vm = nameServer.getMachine(id);
                        prt.print(Printer::Kind::Student, id, 'V', vm->getId());
                    }
                } catch ( WATCardOffice::Lost & ){
                    // lost watcard during creation, no printing msg
                    if (purchased > 0) prt.print(Printer::Kind::Student, id, 'L');
                    thisCard = nullptr;
                    card.reset();
                    // re-create WATCard with $5 balance
                    card = cardOffice.create(id, 5);
                } // outer try
            } // if               
        } // for 
    } // for
    prt.print(Printer::Kind::Student, id, 'F');
}

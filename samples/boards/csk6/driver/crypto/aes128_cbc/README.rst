.. _crypto_sample:

Crypto
######

Overview
********
An example to illustrate the usage of crypto APIs.

Building and Running
********************

Make sure you have the csk6 boards connected over Jlink debuger.

The sample can be built and flashed as follows:

.. code-block:: console

   lisa zep build -b csk6002_9s_nano
   lisa zep flash

Sample Output
=============

.. code-block:: console

   *** Booting Zephyr OS build v1.0.3-alpha.1-1-g1cd2b6612079  ***
   I: AES128 CBC Cipher Sample
   I: Output length (encryption): 32
   I: CBC mode ENCRYPT - Match
   I: Output length (decryption): 32
   I: CBC mode DECRYPT - Match



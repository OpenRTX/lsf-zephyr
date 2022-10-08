.. _gpio_led-sample:

GPIO_LED
######

Overview
********

GPIO_LED is a simple application which blinks an LED forever using the :ref:`GPIO
API <gpio_api>`. The source code shows how to configure GPIO pins as outputs,
then turn them on and off.

See :ref:`pwm-blinky-sample` for a sample which uses the PWM API to blink an
LED.

.. _gpio_led-sample-requirements:

Requirements
************

You will see this error if you try to build gpio_led for an unsupported board:

.. code-block:: none

   Unsupported board: led0 devicetree alias is not defined

The board must have an LED connected via a GPIO pin. These are called "User
LEDs" on many of Zephyr's :ref:`boards`. The LED must be configured using the
``led0`` :ref:`devicetree <dt-guide>` alias. This is usually done in the
:ref:`BOARD.dts file <devicetree-in-out-files>` or a :ref:`devicetree overlay
<set-devicetree-overlays>`.

Building and Running
********************

Build and flash Blinky as follows, changing ``reel_board`` for your board:

.. zephyr-app-commands::
   :zephyr-app: samples/boards/csk6/gpio_led
   :board: reel_board
   :goals: build flash
   :compact:

After flashing, the LED starts to blink. gpio_led does not print to the console.

.. Unity8 Policy Kit's documentation master file, created by
   sphinx-quickstart on Thu Apr  7 09:22:13 2016.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

.. toctree::
   :maxdepth: 2

Overview
========

Unity8 PolicyKit project is a PolicyKit_ agent that is run under
the Unity8 desktop. It displays requests from PolicyKit_ as an
Unity8 Snap Decision. Upon receiving a response it then talks to
PAM_ and verifies the information and sends the auth back to
PolicyKit_.

.. _PolicyKit: https://www.freedesktop.org/wiki/Software/polkit/
.. _PAM: http://www.linux-pam.org/

Architecture
============

This project dependes heavily on ``libpolkit-agent1`` which is provided
by the PolicyKit project and implements the majority of the DBus interface
needed by PolicyKit. It then uses libnotify to communicate with Unit8
and create the snap decision.

Object Interactions
-------------------

.. graphviz:: interactions.dot

Threads
-------

* **Main Thread**. Starts the process and initializes the base objects then
  waits for Unix signals for the application.
* **Agent Thread**. Watches the PolicyKit agent interface and requests
  authentication dialogs based on what PK asks for. It also sends responses
  back to PolicyKit.
* **Authentication Thread**. Managed by the Authentication Manager this thread
  handles all authentication dialogs and session management. It ensures that
  we remain responsive to Unity8's requests.
* **GDBus Threads**. GDBus creates threads that sends messages to the other
  threads based on which bus the messages are on. (system and session)


Primary Classes
===============

These classes make up the majority of how the code works and interacts,
there are a couple of helper classes that are not included but are available
in the source code.

Agent
-----

.. doxygenclass:: Agent
   :project: service
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Authentication Manager
----------------------

.. doxygenclass:: AuthManager
   :project: service
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Authentication
--------------

.. doxygenclass:: Authentication
   :project: service
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Session GLib Interface
----------------------

.. doxygenclass:: Session
   :project: service
   :members:
   :protected-members:
   :private-members:
   :undoc-members:


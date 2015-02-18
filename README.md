Neutrino
========

[![Build Status](https://travis-ci.org/tundra/neutrino.png?branch=master)](https://travis-ci.org/tundra/neutrino)

Neutrino is a new general-purpose programming language. The purpose of neutrino is to create a language, a community, and an ecosystem of tools that serves programmers better than existing languages. At the highest level the goals are:

 - **Give the community control**. As much as possible the community around a code base – corporate, open source, or otherwise – should be in control and make decisions about how to adapt and use the language in a way that's appropriate for that code base.
 - **Take responsibility**. Neutrino aims to have a wide scope and take responsibility not just for the core language but to consider issues like security and the tools around the language.
 - **Make cycles count**. This refers to both programmer cycles and clock cycles.

Those are really abstract goals. These are some of the concrete design directions for achieving them:

- **Language features, including syntax, in libraries**. The core language has little syntax and few conveniences but allows features and syntax to be provided by libraries. Default implementations of the features you'd expect are provided. This allows communities to choose which features to use and gives them the power to develop new features independently of any central authority.
- **Multi-stage meta-programming**. Meta-programming it incredibly powerful. For performance – and sanity – reasons neutrino doesn't allow a running program to modify itself directly. Instead it uses meta-programming in stages, similar to but more general than load-time, link-time, static initialization, in other languages. One stage can construct or modify code for subsequent stages; each stage must be rendered immutable before it is allowed to start executing.
- **No shared-state concurrency**. Neutrino supports E-style concurrency through communicating single-threaded light-weight processes.
- **Strict program control**. Within each process execution is absolutely deterministic, the only possible source of nondeterminism is data obtained by communicating with other processes. Any side-effects a process has outside its own state space also happens through asynchronous messages. If you record a program's initial state and communication you can replay the execution perfectly by executing it again against the recorded communication.
- **A meta-process model**. Since process execution is tightly controlled neutrino can support a powerful meta-process model. *This is something new*. Processes can be replayed in a mode that records not just communication but a full trace of the process' execution, essentially "unfolding" and storing the execution of a process over time. This gives you back-in-time debugging, but the model is more general. For instance, it becomes safe to clone a process into n variations, each with slight differences say different values for a constant, and each clone can continue executing independent of the others. This allows you to observe a range of outcomes side by side. Within this model you can realize the the programming model Bret Victor demonstrates in [Inventing on Principle](http://vimeo.com/36579366) (from ~10:45) – but for real.

All this is possible, the state of the art is almost there. Someone just needs to build it. That's what the neutrino project is for.

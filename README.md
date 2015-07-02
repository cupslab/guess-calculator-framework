# Guess calculator framework

The guess calculator framework was designed to help researchers evaluate the strength of password sets against offline attackers.
It uses an automated, machine-learning approach that processes a training corpus of passwords to learn a simple PCFG that can generate new passwords.
This PCFG is then queried to extract probabilities for passwords, and a lookup table is constructed that represents bundles of passwords in sorted order.
A 2 TB lookup table can correspond to over 700 trillion guesses, or more or less depending on the complexity of the learned grammar and the desired accuracy of the evaluation.

**Please note that this code was written for research purposes for a specific hardware configuration and was not designed with portability in mind.**

This work is derived from tools provided by Matt Weir at https://sites.google.com/site/reusablesec/Home/password-cracking-tools/probablistic_cracker and published at:

    Weir, M., Aggarwal, S., Medeiros, B. d., and Glodek, B. Password cracking using probabilistic context-free grammars. In Proceedings of the 2009 IEEE Symposium on Security and Privacy, IEEE (2009), 391–405.


Please refer to the INSTALL.md, USAGE.md, FAQ.md, and LICENSE files for more information.

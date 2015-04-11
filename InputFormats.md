# HMM State List #
One HMM state symbol per line.

HMM state symbol: `<phone>_<state>`.

E.g. `a_1` for the second state of phone `a`

# Phone List #
One phone symbol per line.

# Phone Lengths #
Format: `<phone> <# states>`

### Example ###
```
a 3
b 3
si 1
```

# Phone Map #
Format: `<phone-from> <phone-to>`

### Example ###
```
a@i a
a@f a
b@i b
b@f b
```

Phone `a@i` is initially mapped to phone `a`, etc.

# Question Sets #
Format: `<question-name> <phone> <phone> ...`

### Example ###
```
CONSONANT-POSTALVEOLAR sh zh
CONSONANT-VELAR k g ng
VOWEL aa ae ah ao aw ax ay ea eh er ey ia ih iy oh ow oy ua uh uw
VOWEL-CHECKED ae ah eh ih oh uh
```

# Samples Text Format #
Header (first line):
> `<version> <feature-dimension> <num-left-contexts> <num-right-contexts>`
Samples (one sample per line):
> `<hmm_definition> <context_definition> <statistics>`
with
  * `<hmm_definition> := <phone> <hmm_state>`
  * `<context_definition> := <context> <context>`
  * `<context> := <phone> ... <phone>`
  * `<statistics> := <weight> <sum> <sum>`
  * `<sum> := <value> ... <value>`
  * `<weight> := float`
  * `<value> := float`
  * `<phone> := string`
  * `<hmm_state> := int`

`<version>` is currently `1`

`<context>` is always from leftmost context phone to right.
Example: state 1 of phone `A` with left context `B C` and right context `D E`:
```
A 1 B C D E ....
```

`<statistics>` is the number of observations, the sum of observed features, and the sum of squared features.
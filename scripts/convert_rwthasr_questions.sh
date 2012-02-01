#!/bin/bash
# convert_rwthasr_questions.sh
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


grep -A1 -e "question description[^/]\+>" | \
  sed -e 's/<question description="\([^"]\+\)">/\1/' \
      -e 's|.*values>\(.*\)</values>|\1|' |\
  awk '(NR % 3 == 1) { d=$1 } (NR % 3 == 2 && $1 != "<key>") { print d, $0 } '


/* Copyright 2019-2020 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "../caffe_importer.h"
#include <hlir/placeholders.h>

using namespace nncase;
using namespace nncase::importer;
using namespace nncase::hlir;
using namespace caffe;

DEFINE_CAFFE_LOWER(Split)
{
    auto &input = *output_tensors_.at(op.bottom(0));

    for (int i = 0; i < op.top_size(); i++)
        output_tensors_.emplace(op.top(i), &input);
}

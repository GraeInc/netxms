/*
** NetXMS - Network Management System
** Copyright (C) 2003-2018 Raden Solutions
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: nn.cpp
**
**/

#include "spe.h"
#include <math.h>

/**
 * Node constructor
 */
NeuralNetworkNode::NeuralNetworkNode(int nextLevelSize)
{
   numWeights = nextLevelSize;
   weights = new double[nextLevelSize];
   weightGradients = new double[nextLevelSize];
   for(int i = 0; i < nextLevelSize; i++)
      weights[i] = static_cast<double>(rand()) / RAND_MAX * (0.001 - 0.0001) + 0.0001;
   bias = static_cast<double>(rand()) / RAND_MAX * (0.001 - 0.0001) + 0.0001;
   biasGradient = 0.0;
   value = 0.0;
}

/**
 * Node destructor
 */
NeuralNetworkNode::~NeuralNetworkNode()
{
   delete weights;
   delete weightGradients;
}

/**
 * Reset node before training
 */
void NeuralNetworkNode::reset()
{
   for(int i = 0; i < numWeights; i++)
      weightGradients[i] = 0.0;
   biasGradient = 0.0;
}

/**
 * Constructor
 */
NeuralNetwork::NeuralNetwork(int inputCount, int hiddenCount) :
      m_input(inputCount, 8, true), m_hidden(hiddenCount, 8, true), m_output(1)
{
   for(int i = 0; i < inputCount; i++)
      m_input.add(new NeuralNetworkNode(hiddenCount));
   for(int i = 0; i < hiddenCount; i++)
      m_hidden.add(new NeuralNetworkNode(1));
   m_mutex = MutexCreate();
}

/**
 * Destructor
 */
NeuralNetwork::~NeuralNetwork()
{
   MutexDestroy(m_mutex);
}

/**
 * Compute input value
 */
double NeuralNetwork::normalize(double input)
{
   //if(m_maxValue == m_minValue || (m_maxValue < 1 && m_minValue < -1))
      return input;
   //return (input - m_minValue)/(m_maxValue - m_minValue);
}

/**
 * Compute output value
 */
double NeuralNetwork::removeNormalization(double input)
{
   //if(m_maxValue == m_minValue || (m_maxValue < 1 && m_minValue < -1))
      return input;
   //return input * (m_maxValue - m_minValue) + m_minValue;
}

double ourThan(double x)
{
   if (x < -20.0) return -1.0; // approximation is correct to 30 decimals
   else if (x > 20.0) return 1.0;
   return tanh(x);
}

/**
 * Compute output value
 */
double NeuralNetwork::computeOutput(double *inputs)
{
   for(int i = 0; i < m_input.size(); i++)
      m_input.get(i)->value = normalize(inputs[i]);

   double os = 0.0;
   for(int i = 0; i < m_hidden.size(); i++)
   {
      double s = 0.0;
      for(int j = 0; j < m_input.size(); j++)   // compute i-h sum of weights * inputs
      {
         s += m_input.get(j)->value * m_input.get(j)->weights[i];
      }
      s += m_hidden.get(i)->bias;

      double v = ourThan(s);
      m_hidden.get(i)->value = v;

      os += v * m_hidden.get(i)->weights[0]; // update h-o sum of weights * hidden
   }


   os += m_output.bias; // add bias to output sum
   m_output.value = os;
   return removeNormalization(os);
}

/**
 * Shuffle array
 */
static void Shuffle(int *data, int size)
{
   for(int i = 0; i < size - 1; i++)
   {
      int idx = i + rand() % (size - i);
      int t = data[idx];
      data[idx] = data[i];
      data[i] = t;
   }
}
/**
 * Train network using given data series
 */
double NeuralNetwork::accuracy(double *series, size_t length, double howClose)
{
   int numCorrect = 0;
   int numWrong = 0;
   for(int i = 0; i < static_cast<int>(length) - m_input.size(); i++)
   {
      double expectedResult = series[(m_input.size()+1)*i + m_input.size()];
      double actualResult = computeOutput(series+(m_input.size()+1)*i);


      if (abs(expectedResult - actualResult) < howClose)  // within 30
        ++numCorrect;
      else
        ++numWrong;
   }

   return (numCorrect * 1.0) / (numCorrect + numWrong);
}

void NeuralNetwork::showWeights()
{
   for(int i = 0; i < m_hidden.size(); i++)
   {
      double s = 0.0;
      for(int j = 0; j < m_input.size(); j++)   // compute i-h sum of weights * inputs
      {
         nxlog_debug(2, _T("%f, "), m_input.get(j)->weights[i]);
      }
      nxlog_debug(2, _T("\n"));
      nxlog_debug(2, _T("bias: "), m_hidden.get(i)->bias);

      double v = ourThan(s);
      m_hidden.get(i)->value = v;
      nxlog_debug(2, _T("hiden: %f"), m_hidden.get(i)->weights[0]);
   }
   nxlog_debug(2, _T("bias: %f"), m_output.bias);
}

void NeuralNetwork::setDataRange(double min, double max)
{
   m_minValue = min;
   m_maxValue = max;
}

/**
 * Train network using given data series
 */
void NeuralNetwork::train(double *series, size_t length, int rounds, double learnRate)
{
   if (length <= m_input.size())
      return;  // Series is too short

   // Prepare training data
   int blockCount = length;
   int blockSize = m_input.size() + 1;
   /*
   for(int i = 0; i < blockCount; i++)
   {
      for(int j = 0; j < blockSize; j++)
      {
         series[i*blockSize+j] = normalize(series[i*blockSize+j]);
      }
   }
   */

   double *hSignals = new double[m_hidden.size()];

   for(int i = 0; i < m_input.size(); i++)
      m_input.get(i)->reset();
   for(int i = 0; i < m_hidden.size(); i++)
      m_hidden.get(i)->reset();
   m_output.reset();

   // Processing sequence
   int *sequence = new int[blockCount];
   for(int i = 0; i < blockCount; i++)
      sequence[i] = i;
   nxlog_debug(2, _T("bc = %d"), blockCount);

   while(rounds-- > 0)
   {
      Shuffle(sequence, blockCount); // visit each block in random order
      for(int i = 0; i < blockCount; i++)
      {
         double *block = series+(sequence[i]*blockSize);
         double target = block[m_input.size()];
         computeOutput(block);

         // 1. compute output node signal
         double errorSignal = target - m_output.value;

         if((rounds % 2000) == 0 && (i == 0 || i ==1))
         {
            nxlog_debug(2, _T("sequence[%d] = %d"), i, sequence[i]);
            nxlog_debug(2, _T("epoch = %d, error = %lf"), rounds, errorSignal);
            nxlog_debug(2, _T("target = %lf, out = %lf"), target,  m_output.value);
            nxlog_debug(2, _T("0 = %lf, 1 = %lf, 2 = %lf"), block[0], block[1], block[2]);
         }

         // 2. compute hidden-to-output weight gradients using output signals
         for(int j = 0; j < m_hidden.size(); j++)
            m_hidden.get(j)->weightGradients[0] = errorSignal * m_hidden.get(j)->value;

         // 3. compute output bias gradients using output signals
         m_output.biasGradient = errorSignal;

         // 4. compute hidden node signals
         for(int j = 0; j < m_hidden.size(); j++)
         {
            NeuralNetworkNode *n = m_hidden.get(j);
            double derivative = (1 + n->value) * (1 - n->value);
            hSignals[j] = derivative * n->weights[0] * errorSignal; //Check if it is correct
         }

         // 5. compute input-hidden weight gradients
         for(int j = 0; j < m_input.size(); j++)
         {
            NeuralNetworkNode *n = m_input.get(j);
            for(int k = 0; k < m_hidden.size(); k++)
               n->weightGradients[k] = hSignals[k] * n->value;
         }

         // 6. compute hidden node bias gradients
         for(int j = 0; j < m_hidden.size(); j++)
            m_hidden.get(j)->biasGradient = hSignals[j];

         // 7. update input-to-hidden weights
         for(int j = 0; j < m_input.size(); j++)
         {
            NeuralNetworkNode *n = m_input.get(j);
            for(int k = 0; k < m_hidden.size(); k++)
               n->weights[k] += n->weightGradients[k] * learnRate;
         }

         // 8. update hidden biases and hidden-to-output weights
         for(int j = 0; j < m_hidden.size(); j++)
         {
            NeuralNetworkNode *n = m_hidden.get(j);
            n->bias += n->biasGradient * learnRate;
            n->weights[0] += n->weightGradients[0] * learnRate;
         }

         // 9. update output node bias
         m_output.bias += m_output.biasGradient * learnRate;
      }
   }

   delete[] hSignals;
   delete[] sequence;
}

/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2010 Victor Kirhenshtein
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
package org.netxms.client.datacollection;

import java.util.Date;

/**
 * Single row in DCI data
 * @author Victor
 *
 */
public class DciDataRow
{
	private Date timestamp;
	private Object value;
	private Object rawValue;
   private Object predictedValue;

	public DciDataRow(Date timestamp, Object value)
	{
		super();
		this.timestamp = timestamp;
		this.value = value;
		this.rawValue = null;
      this.predictedValue = null;
	}

	/**
	 * Set raw value
	 * 
	 * @param rawValue new raw value
	 */
	public void setRawValue(Object rawValue)
	{
	   this.rawValue = rawValue;
	}

   /**
    * Set predicted value
    * 
    * @param rawValue new raw value
    */
   public void setPredictedValue(Object predictedValue)
   {
      this.predictedValue = predictedValue;
   }
	
	/**
	 * @return the timestamp
	 */
	public Date getTimestamp()
	{
		return timestamp;
	}

	/**
	 * @return the value
	 */
	public Object getValue()
	{
		return value;
	}
	
	/**
	 * @return the value
	 */
	public String getValueAsString()
	{
		return (value != null) ? value.toString() : "";
	}

	/**
	 * @return the value
	 */
	public long getValueAsLong()
	{
		if (value instanceof Long)
			return ((Long)value).longValue();

		if (value instanceof Double)
			return ((Double)value).longValue();
		
		return 0;
	}

	/**
	 * @return the value
	 */
	public double getValueAsDouble()
	{
		if (value instanceof Long)
			return ((Long)value).doubleValue();

		if (value instanceof Double)
			return ((Double)value).doubleValue();
		
		return 0;
	}

   /**
    * @return raw value
    */
   public Object getRawValue()
   {
      return rawValue;
   }
   
   /**
    * @return raw value as string
    */
   public String getRawValueAsString()
   {
      return (rawValue != null) ? rawValue.toString() : "";
   }

   /**
    * @return raw value as long
    */
   public long getRawValueAsLong()
   {
      if (rawValue instanceof Long)
         return ((Long)rawValue).longValue();

      if (rawValue instanceof Double)
         return ((Double)rawValue).longValue();
      
      return 0;
   }

   /**
    * @return raw value as double
    */
   public double getRawValueAsDouble()
   {
      if (rawValue instanceof Long)
         return ((Long)rawValue).doubleValue();

      if (rawValue instanceof Double)
         return ((Double)rawValue).doubleValue();
      
      return 0;
   }  
   
   /**
    * @return raw value
    */
   public Object getPredictedValue()
   {
      return predictedValue;
   }

   
   /**
    * @return raw value as string
    */
   public String getPredictedValueAsString()
   {
      return (predictedValue != null) ? predictedValue.toString() : "";
   }

   /**
    * @return raw value as long
    */
   public long getPredictedValueAsLong()
   {
      if (predictedValue instanceof Long)
         return ((Long)predictedValue).longValue();

      if (predictedValue instanceof Double)
         return ((Double)predictedValue).longValue();
      
      return 0;
   }

   /**
    * @return raw value as double
    */
   public double getPredictedValueAsDouble()
   {
      if (predictedValue instanceof Long)
         return ((Long)predictedValue).doubleValue();

      if (predictedValue instanceof Double)
         return ((Double)predictedValue).doubleValue();
      
      return 0;
   }  
	
   /**
    * Invert value
    */
   public void invert()
   {
      if (value instanceof Long)
         value = -((Long)value);
      else if (value instanceof Double)
         value = -((Double)value);
   }

   /* (non-Javadoc)
    * @see java.lang.Object#toString()
    */
   @Override
   public String toString()
   {
      return "DciDataRow [timestamp=" + timestamp + ", value=" + value + ", rawValue=" + rawValue + ", predictedValue=" + predictedValue + "]";
   }
}

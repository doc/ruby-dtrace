#
# Ruby-Dtrace
# (c) 2007 Chris Andrews <chris@nodnol.org>
#
# A DTrace record for the formatted part of a printf() action. 
class Dtrace
  class PrintfRecord
    attr_accessor :value
  end
end

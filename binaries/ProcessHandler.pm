# Methods for handling subprocesses such as those created by fork()
#
# Written by Saranga Komanduri and Tim Vidas
package ProcessHandler;

use strict;
use warnings;
use POSIX qw(:signal_h :errno_h :sys_wait_h);

sub new {
  # Constructor -- mostly Perl OO boilerplate
  my $class = shift;
  my $self  = {
    childAlive => {},   # empty hash for storing child status
                        # hash is populated when processes are created by fork
           # each child will be marked as "alive" ('1') or "dead" ('0')
           # '0' evaluates to false in Perl
    improperTermination => 0
  };
  return bless $self, $class;
}

# Simple setters
sub setAlive {
  my ($self, $mypid) = @_;
  $self->{childAlive}{$mypid} = '1';
}

sub setDead {
  my ($self, $mypid) = @_;
  $self->{childAlive}{$mypid} = '0';
}

sub REAPER {
  # Zombie reaper (the SIGCHLD handler)
  # Assign as SIGCHLD handler using $SIG{CHLD} = sub { $thisobject->REAPER() };
  my $self = shift;
  my $mypid;
  $mypid = waitpid(-1, &WNOHANG);

  # From http://www.perlmonks.org/?node_id=801710
  my $status      = $?;
  my $wifexited   = WIFEXITED($status);
  my $wexitstatus = $wifexited ? WEXITSTATUS($status) : undef;
  my $wifsignaled = WIFSIGNALED($status);
  my $wtermsig    = $wifsignaled ? WTERMSIG($status) : undef;
  my $wifstopped  = WIFSTOPPED($status);
  my $wstopsig    = $wifstopped ? WSTOPSIG($status) : undef;

  if ($mypid <= 0) {
    # no child waiting.  Ignore it.
  }
  # Else check for either non-zero exit status or death by signal
  elsif ($wifexited && $wexitstatus != 0 || $wifsignaled) {
    $self->setDead($mypid);
    $self->{improperTermination} = 1;
    print STDERR "Child process $mypid returned non-zero exit status or termination by signal!\n";
  }
  elsif ($wifexited) {
    # $lock->down;
    $self->setDead($mypid)
      ; # Note that this may add a new process to the hash that was not added using setAlive or setDead, which is the desired behavior
        # $lock->up;
    # print STDERR "sub process $mypid exited.\n";    
  }
  else {
    if ($mypid) {
      print STDERR "False alarm on $mypid: ";
      if (WIFSIGNALED($?)) {
        my $insig = WTERMSIG($?);
        print STDERR "process terminated by signal $insig.\n";
      } elsif (WIFSTOPPED($?)) {
        my $insig = WSTOPSIG($?);
        print STDERR "process stopped by signal $insig.\n";
      } else {
        print STDERR "Unable to determine return code: $? \n";
      }
    }
  }
  # $SIG{CHLD} = sub { $self->REAPER() };    # in case of unreliable signals
}

sub isDead {
  # If this process has been marked as dead, delete it from childAlive and return 1
  my ($self, $mypid) = @_;
  my $retval = 0;
  # $lock->down;
  if (exists $self->{childAlive}{$mypid}) {
    if (!$self->{childAlive}{$mypid}) {
      delete($self->{childAlive}{$mypid});
      $retval = 1;
    }
  }
  # $lock->up;
  return $retval;
}

sub aliveCount {
  # After checking status for all processes, return the number of child processes that are now alive
  my $self      = shift;
  my $reapcount = 0;
  my $count     = 0;
  # $lock->down;
  while ((my $key, my $value) = each($self->{childAlive})) {
    if ($value) {
      my $mypid = waitpid($key, &WNOHANG);
      if (($mypid == -1) || 
          ($mypid == $key) && (WIFEXITED($?))) {
        $self->setDead($mypid);
        $reapcount++;
      }
      else {
        $count++;
      }
    }
  }
  # $lock->up;
  print STDERR "$reapcount processes reaped manually*****\n" unless $reapcount == 0;
  return $count;
}

sub killAll {
  # Used to kill processes in case of exceeded max time or SIGINT
  my $self = shift;
  while ((my $key, my $value) = each($self->{childAlive})) {
    print STDERR "Killing process group $key with SIGTERM\n";
    kill -15, $key;
    waitpid($key, &WNOHANG);
  }
}

sub checkChildErrorAndExit {
  # Check if the improper termination flag was set when some child exited
  my $self = shift;
  if ($self->{improperTermination}) {
    print STDERR "Detected improper termination in child, aborting...\n";
    # This should kill all child processes on exit, but for some reason it
    # does not.  I am unable to figure out why, but at least it accomplishes
    # the intended behavior of sending an errorlevel back to the parent when
    # the child processes are killed or complete on thier own.
    die "Detected child process terminated improperly! " .
      "Check logs for error messages!\n";    
  }
}


1;
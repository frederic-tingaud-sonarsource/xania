>rand_prog 5~
grumble
say Litterbugs.
if rand(20)
	say All I do each day is cleanup other people's messes.
	if rand(20)
		say I do not get paid enough.
	else
		if rand(20)
			say Day in.  Day out.  This is all I do 24 hours a day.
			if rand(10)
				yell I want a vacation!
			endif
		endif
	endif
endif
~
>act_prog drops~
if rand(50)
	noogie $n
else
	slap $n
	say Save the planet!
endif
~
|

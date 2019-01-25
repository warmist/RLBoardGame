deck={}
print("Actually running lua code!")
deck.strike={
	--main
	name="Strike",
	cost=2,
	description="Perform an attack with a very big sword",
	--misc
	attack=3,
	range=0,
	--callbacks
	use=function ( card,game )
		local target=game.target_enemy(game.player,card.range)
		game.damage(target,card.attack)
	end
}
deck.push={

	name="Push",
	cost=1,
	description="Force a move. Deal damage if can't move",

	attack=1,
	range=0,
	distance=3,

	use=function ( card,game )
		--first ask which one of enemies you want to push
		local target=game.target_enemy(game.player,card.range)
		--figure out the direction of push
		local dir=target.pos-game.player.pos
		dir=dir/dir:lenght()
		local start=target.pos
		--raycast (i.e. like going straight in that direction)
		local path=game.raycast(start+dir,dir,card.distance-1,target)
		--move the unit
		game.move(target,path)
		--then if the unit could not move ALL the way, hurt it
		if #path<card.distance-1 then
			game.damage(target,card.attack)
		end
	end
}
deck.move={
	name="Run",
	cost=1,
	description="Run to the target",

	generated=true,
	range=5,
	move=true,

	use=function (card,game)
		local path=game.target_path(game.player,card.range)
		game.move(game.player,path)
	end
}
return deck